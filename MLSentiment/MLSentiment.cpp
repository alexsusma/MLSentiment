#include <algorithm>
#include "MLSentiment.h"
#include "document.h"
#include "writer.h"
#include "stringbuffer.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <conio.h>
#include "TwitterConnection.h"

//sleep time between subsequent excutions
#define EXECUTION_PERIOD	10000

//define this flag if you wish to run multihtreaded
#define MULTITHREADING

//define this flag if you wish to output a raw list of tweets analyzed
#define OUTPUT_TWEETS

//define this flag if you wish to input company names manually
//#define MANUAL_INPUT 

using namespace rapidjson;
using namespace std;



/*	GetInput will ask the user for the names of the companies to be researched.
*	the function will then parse the input and store the company names in a vector
*	of strings. A count of tweets to be analyzed per company is also requested.
*	The function can also read the input from an input file inputCompanies.txt
*/
bool GetInput(vector<string>& aQueries, int& aCount)
{

#ifdef MANUAL_INPUT
	cout<<"\nEnter company names separated by comma(i.e. Apple,Google,Netflix...):\n\n";
	char tmpBuf[512] ={0};
	cin.getline(tmpBuf,sizeof(tmpBuf));
	
	//parse the input into individual company names and store them
	char * token;
	char * context;
    token = strtok_s (tmpBuf,",",&context);
	while (token != NULL)
	{
		aQueries.push_back(string(token));
		token = strtok_s (NULL, ",",&context);
	}
	if (aQueries.empty()){
		cout<<"Input file not loaded !!!";
		return false;
	}

	cout<<"\nHow many tweets to analyze per company ?\n\n";
	cin >> aCount;	
#else
	//read input from file
	string temp;
	ifstream fileIn("inputCompanies.txt");
	cout<<"\nReading input...\n";
	fileIn>>aCount;
	if(!aCount){
		cout<<"\nPlease check the first line of the input file and make sure it is an integer!\n";
		return false;
	}
	
	while (fileIn >> temp){
		aQueries.push_back(temp);
		cout<<temp<<",";
	}
	fileIn.close();

	cout<<"\n"<<aCount<<" tweets each\n\n";

	if (aQueries.empty()){
		cout<<"Input file not loaded !!!";
		return false;
	}
#endif
	return true;
}

/*	LoadDictionaries will read in 3 text files and store the contents 
*	in 3 string vectors. The vectors will contain english positive,
*	negative and stop words.
*/
bool LoadDictionaries()
{
	string temp;

	ifstream fileIn("positive-words.txt");
	while (fileIn >> temp)
		positiveWords.push_back(temp);
	fileIn.close();

	ifstream fileInNeg("negative-words.txt");
	while (fileInNeg >> temp)
		negativeWords.push_back(temp);
	fileInNeg.close();

	ifstream fileInStop("stop-words.txt");
	while (fileInStop >> temp)
		stopWords.push_back(temp);
	fileInNeg.close();

	if (positiveWords.empty() || negativeWords.empty() || stopWords.empty()){
		cout<<"Dictionaries could not be loaded. Please make sure "<< 
			"stop-words.txt, negative-words.txt and positive-words.txt "<<
			"are present in the execution folder.\n";
		return false;
	}

	return true;
}


/*	SplitTweet will take a string containing a tweet and break it down into 
*	words based on the delimiter.
*	Return value is a vector of strings which contains the tweet words.
*/
vector<string> ParseTweet(const string &tweet, char delimiter) {
    vector<string> words;
    stringstream sstream(tweet);
    string word;

    while (getline(sstream, word, delimiter)) {
        words.push_back(word);
    }
    return words;
}


/*	AnalyzeTweetSentiment will take a string containing a tweet and analyze 
*	the sentiment based on words. The function will go through the words of 
*	the tweet and check them against the stop words which will be ignored.
*	If the word is not a stop word, it will be checked against positive and 
*	negative words. If there is a match the count for that specific sentiment 
*	is increased. 
*	Return value is the absolute difference between the positive and the negative counts.
*/
int AnalyzeTweetSentiment(const string& tweet)
{
	int posCount = 0, negCount = 0;
	vector<string> words = ParseTweet(tweet,' ');
				
	for (auto it : words){
		//skip stop words
		if (binary_search(stopWords.begin(), stopWords.end(), it))
			continue;
		
		if (binary_search(positiveWords.begin(), positiveWords.end(), it))
			++posCount;
		else if (binary_search(negativeWords.begin(), negativeWords.end(), it))
			--negCount;
	}
	//return difference between positive and negative words
	return posCount-abs(negCount);
}


/*	ProcessTweet will take a string containing a tweet and preprocess it in
*	preparation for analysis. The function will perform the following operations:
*	1. remove the # symbol and other special characters
*	2. make lowercase
*	3. remove @username
*	4. remove http links
*	Note: The function used to remove "RT" string as well. It was moved in the list of stop words
*/
void ProcessTweet(string& tweet)
{
	//remove hashtags and other special characters
	size_t position = 0;

	string specialCharacters = "+\"\'%!()[]{}:.;<>/\\=^&*~#?$1234567890";
	for (position = tweet.find_first_of(specialCharacters); 
			position != string::npos; position = tweet.find(specialCharacters, position))
	{
			tweet.erase(position, 1);
	}
	
	//make lowercase
	for (string::size_type i=0; i<tweet.length(); ++i)
		tweet.at(i) = tolower(tweet[i]);
	
	//remove @username
	position = 0;

	for (position = tweet.find_first_of("@"); 
			position != string::npos; position = tweet.find(specialCharacters, position))
	{
		if (size_t endUsername = tweet.find_first_of(": ,",position) != string::npos)
			tweet.erase(position, endUsername-position+1);
		else
			tweet.erase(position, tweet.length()-position+1);
	}

	//remove url
	position = 0;

	for (position = tweet.find("http"); 
			position != string::npos; position = tweet.find("http", position))
	{
		if (size_t endLink = tweet.find_first_of(": ,",position) != string::npos)
			tweet.erase(position, endLink-position+1);
		else
			tweet.erase(position, tweet.length()-position+1);
	}
}




/*	AnalyzeCompanySentiment will a company name and then access its tweets
*	to analyze and compute the final sentiment counts for that company. 
*/
void AnalyzeCompanySentiment(string& aCompanyName)
{
	SentimentStatus oldStatus = ssNeutral;
	CompanyResults& company = companyResultsMap[aCompanyName.c_str()];

	if (company.hasPrevResults){
		//extract previous results for comparison
		oldStatus		= company.status;
		company.sentimentsPerTweet.clear();
		company.positive.count = 0;
		company.neutral.count = 0;
		company.negative.count = 0;
		company.status = ssNeutral;
	}

	//go through all the tweets
	for (auto it : company.tweets){
		string tweet = it;
		ProcessTweet(tweet);
		company.sentimentsPerTweet.push_back(AnalyzeTweetSentiment(tweet));
	}	

	
	vector<int>& sentiments = company.sentimentsPerTweet;

	//sum up all the individual sentiments to come up with the final counts
	for (unsigned int i = 0; i < sentiments.size(); ++i)
	{
		int sentiment = sentiments[i];
		if (sentiment > 0)
			++company.positive.count;
		else if (sentiment == 0)
			++company.neutral.count;
		else if (sentiment < 0)
			++company.negative.count;
	}

	
	company.status = company.positive.count > company.negative.count + tweetsCount/10 ? ssPositive : (company.positive.count + tweetsCount/10 < company.negative.count ? ssNegative : ssNeutral);
	

	//check results against last set of results
	if (company.hasPrevResults)
		company.statusChanged = company.status != oldStatus;
		
	company.hasPrevResults = true;					
}


/*	RunSearchAndAnalysis will start a query for a company name with a tweet count.
*	Once it has the results, it will parse them into tweets and pass them to the
*	analyzer function.
*/
void RunSearchAndAnalysis(string aQuery, string aCount)
{
	string replyMsg;
	twitCurl* twit = twitterObj.clone();
	CompanyResults& company = companyResultsMap[aQuery.c_str()];

	//replace any spaces with %20 for compatibility with http query
	// NOTE: THIS STILL DOES NOT WORK IN THE QUERY
	size_t position = 0;
	for (position = aQuery.find(' '); position != string::npos; position = aQuery.find(' ', position)){
		//just delete leading and trailing spaces
		if (position == 0 || position == aQuery.length()-1)
			aQuery.erase(position,1);
		else
			//replace middle spaces by %20
			aQuery.replace(position, 1, "%20");
	}
	

	//run the search and extract the tweets
    if( twit->search( aQuery,  aCount ) )
    {
		//RapidJSON doc parser
		Document doc;
		
		//get the latest reply and parse it
        twit->getLastWebResponse( replyMsg );
		doc.Parse(replyMsg.c_str());

		//if tweets have been found
		if (doc.HasMember("statuses")){

			int tweetsNum = doc["statuses"].Size();

			company.name = aQuery.c_str();

			//clear previous tweets if we have any
			if (company.hasPrevResults)
				company.tweets.clear();

			//extract and store tweets
			for (int i = 0 ; i < tweetsNum; ++i){
				company.tweets.push_back(doc["statuses"][i]["text"].GetString());	
			}

			AnalyzeCompanySentiment(company.name);
		}
    }
    else
    {
        twit->getLastCurlError( replyMsg );
        cout<<"\ntwitCurl::search error:\n"<<replyMsg.c_str();
    }
}


/*	OutputResults will output final sentiment counts to the console 
*	and a CSV file
*/
void OutputResults()
{
	stringstream out;
	ofstream myfile;
	string fileName = ("OutputResults.csv");
	myfile.open (fileName.c_str());

	cout<<"Company\t\t\tPos\tNeut\tNeg\tSentiment\tChange\n";
	cout<<"____________________________________________________________________________\n\n";
	out<<"Company,Positive,Neutral,Negative\n";
	//output to screen
	for (auto it : companyResultsMap){
		CompanyResults company = it.second;

		string tabs = it.first.length() > 8 ? "\t\t" : "\t\t\t";

		cout<<it.first.c_str() << tabs<<company.positive.count<<
									"\t"<<company.neutral.count<< 
									"\t"<<company.negative.count;
		switch(company.status){
			case ssPositive: cout<<"\tPositive"; tabs = "\t";		break;
			case ssNegative: cout<<"\tNegative"; tabs = "\t";		break;
			case ssNeutral:  cout<<"\tNeutral";  tabs = "\t\t";	break;
		}


		if (company.statusChanged)
			cout<<tabs<<"Yes";

		cout<<"\n";

		out<<it.first.c_str()<< ","<<company.positive.count<<","<<company.neutral.count<<","<<company.negative.count<<"\n";

	}
	myfile << out.rdbuf();
	myfile.close();

#ifdef OUTPUT_TWEETS
	//print out all the raw tweets for debugging. Since most raw tweets contain ',', 
	//output the tweets in a txt file, not csv.
	//Format:  tweet,sentiment score
	stringstream out2;
	ofstream myfile2;
	fileName = ("tweets.txt");
	myfile2.open (fileName.c_str());
	for (auto it : companyResultsMap){
		CompanyResults companyRes = it.second;
		int i = 0;
		out2<<companyRes.name<<"\n\n";
		for (auto tweet: companyRes.tweets){
			out2<<tweet<<","<<companyRes.sentimentsPerTweet[i++]<<"\n";
		}
		out2<<"\n\n\n\n";

	}

	myfile2 << out2.rdbuf();
	myfile2.close();
#endif
}

int main( int argc, char* argv[] )
{

	//reserve memory to avoid constant resizing
	positiveWords.reserve(5000);
	negativeWords.reserve(5000);
	stopWords.reserve(5000);

	//Load dictionaries from files and ask user for input
	// (or read the input file based on the definition of MANUAL_INPUT)
	vector<string> inputQueries;
	tweetsCount = 0;

	cout <<"=======================================\n";
	cout<<"Twitter Sentiment Analyzer by Alex Susma\n";
	cout<<"June 2016\n";
	cout <<"=======================================\n\n";

	if (!LoadDictionaries() || !GetInput(inputQueries, tweetsCount)){
		cout<<"Press any key to end the program";
		_getch();
		return 0;
	}


	//set username & password
    string userName ="alexsusma";
    string passWord ="Fy6#@A%9";

	//Connect to Twitter
    if (!ConnectToTwitter(userName, passWord, twitterObj)){
		cout<<"Connection failed. Program exiting...";
		return 0;
	}


	while(1){

		clock_t start = clock();


#ifdef MULTITHREADING

		//start search and analyse results in separate threads
		vector<thread> myThreads;
		myThreads.reserve(inputQueries.size());

		for (unsigned int i = 0; i < inputQueries.size();++i){
			myThreads.push_back(thread(RunSearchAndAnalysis, inputQueries[i],to_string(tweetsCount)));
		}

		
		//wait for all threads to finish
		for (auto &it : myThreads){
			it.join();
		}

		double diff = (clock() - start) / (double)CLOCKS_PER_SEC;
		cout<<"\nTotal time multithreading: "<<diff<<"\n\n";

#else
		// FOR PERFORMANCE TESTING PURPOSES
	
		//sequential execution 1 thread
		start = clock();
		for (unsigned int i = 0; i < inputQueries.size();++i){
			RunSearchAndAnalysis(inputQueries[i],to_string(tweetsCount));
		}

		double diff = (clock() - start) / (double)CLOCKS_PER_SEC;
		cout<<"\nTotal time 1 thread: "<<diff<<"\n\n";

#endif
		OutputResults();

		cout << "\nNext analysis in "<<EXECUTION_PERIOD/1000<<" seconds";
		Sleep(EXECUTION_PERIOD);	
}

    return 0;
}

