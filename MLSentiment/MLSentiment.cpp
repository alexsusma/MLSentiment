#include <algorithm>
#include "MLSentiment.h"
#include "document.h"
#include "writer.h"
#include "stringbuffer.h"
#include <iostream>
#include <fstream>
#include "pointer.h"
#include <thread>
#include <conio.h>
#include "TwitterConnection.h"


#define EXECUTION_PERIOD	10000
#define MAX_THREADS			10
#define MULTITHREADING
//#define MANUAL_INPUT 

using namespace rapidjson;
using namespace std;



/*	GetInput will ask the user for the names of the companies to be researched.
*	the function will then parse the input and store the company names in a vector
*	of strings. A count of tweets to be analyzed per company is also requested.
*	The function can also read the input from an input file inputCompanies.txt
*/
void GetInput(vector<string>& aQueries, int& aCount)
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


	cout<<"\nHow many tweets to analyze per company ?\n\n";
	cin >> aCount;	
#else
	//read input from file
	string temp;
	ifstream fileIn("inputCompanies.txt");
	cout<<"\nReading input...\n";
	fileIn>>aCount;
	while (fileIn >> temp){
		aQueries.push_back(temp);
		cout<<temp<<",";
	}
	cout<<"  "<<aCount<<" twets each\n";
	fileIn.close();
	
#endif
}

/*	LoadDictionaries will read in 3 text files and store the contents 
*	in 3 string vectors. The vectors will contain english positive,
*	negative and stop words.
*/
void LoadDictionaries()
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

	if (positiveWords.size() == 0 || negativeWords.size() == 0 || stopWords.size() == 0)
		cout<<"Dictionaries not loaded !!!";

}


/*	SplitTweet will take a string containing a tweet and break it down into 
*	words based on the delimiter.
*	Return value is a vector of strings which contains the tweet words.
*/
vector<string> SplitTweet(const string &tweet, char delimiter) {
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
*	If the word is not a stop word, it iwll be checked against positive and 
*	negative words. If there is a match the count for that specific sentiment 
*	is increased. 
*	Return value is the absolute difference between the positive and the negative counts.
*/
int AnalyzeTweetSentiment(const string& tweet)
{
	int posCount = 0, negCount = 0;
	vector<string> words = SplitTweet(tweet,' ');
				
	for (vector<string>::iterator it = words.begin(); it != words.end(); ++it){
		//skip stop words
		if (binary_search(stopWords.begin(), stopWords.end(), *it))
			continue;
		
		if (binary_search(positiveWords.begin(), positiveWords.end(), *it))
			++posCount;
		else if (binary_search(negativeWords.begin(), negativeWords.end(), *it))
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
	/*size_t start;
	while(position != string::npos) {
		start = tweet.find_first_of("+\"%!()[]{}:;<>/\\=^&*~#?$1234567890", position);
		if (start != string::npos)
			tweet.replace(start,1,"");
		else
			break;
		if(position != string::npos)
			position++;
	}
	*/
	string specialCharacters = "+\"%!()[]{}:;<>/\\=^&*~#?$1234567890";
	for (position = tweet.find_first_of(specialCharacters); 
			position != string::npos; position = tweet.find(specialCharacters, position))
	{
			tweet.erase(position, 1);
	}
	
	//make lowercase
	for (std::string::size_type i=0; i<tweet.length(); ++i)
		tweet.at(i) = tolower(tweet[i]);
	
	//remove @username
	position = 0;
	/*start = 0;
	while(position != string::npos) {
		start = tweet.find("@", position);
		if (start != string::npos){
			size_t end = tweet.find_first_of(": ,",start);
			if (end != string::npos)
				tweet.replace(start,end-start+1,"");
			else
				tweet.replace(start,tweet.length()-start+1,"");
		}
		else
			break;
		if(position != string::npos)
			position = start;
	}
	*/
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
	/*start = 0;
	while(position != string::npos) {
		start = tweet.find("http", position);
		if (start != string::npos){
			size_t end = tweet.find_first_of(" ",start);
			if (end != string::npos)
				tweet.replace(start,end-start+1,"");
			else
				tweet.replace(start,tweet.length()-start+1,"");
		}
		else
			break;
		if(position != string::npos)
			position++;
	}
	*/
	for (position = tweet.find("http"); 
			position != string::npos; position = tweet.find("http", position))
	{
		if (size_t endLink = tweet.find_first_of(": ,",position) != string::npos)
			tweet.erase(position, endLink-position+1);
		else
			tweet.erase(position, tweet.length()-position+1);
	}
}


/*	AnalyzeCompanySentiment will take a vector of strings containing all the tweets
*	associated with a company and compute the final sentiment counts. 
*/
void AnalyzeCompanySentiment(string& aCompanyName, vector<string>& tweets)
{
	CompanyResults& myResults = companyResultsMap[aCompanyName.c_str()];

	for (vector<string>::iterator it = tweets.begin(); it != tweets.end(); ++it)
	{
		string tweet = *it;
		ProcessTweet(tweet);
		myResults.sentimentsPerTweet.push_back(AnalyzeTweetSentiment(tweet));

	}	


	vector<int>& sentiments = myResults.sentimentsPerTweet;
	for (unsigned int i = 0; i < sentiments.size(); ++i)
	{
		int sentiment = sentiments[i];
		if (sentiment > 0)
			++myResults.positiveCount;
		else if (sentiment == 0)
			++myResults.neutralCount;
		else if (sentiment < 0)
			++myResults.negativeCount;
	}
				
}


/*	RunSearchAndAnalysis will start a query for a company name with a tweet count.
*	Once it has the results, it will parse them into tweets and pass them to the
*	analyzer function.
*/
void RunSearchAndAnalysis(string aQuery, string aCount)
{
	
	string replyMsg;
	vector<string> tweets;
	twitCurl* twit = twitterObj.clone();

	//replace any spaces with %20 for compatibility with http query
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
        twit->getLastWebResponse( replyMsg );
	
		Document doc;
		doc.Parse(replyMsg.c_str());

		if (doc.HasMember("statuses")){

			int tweetsNum = doc["statuses"].Size();

			companyResultsMap[aQuery.c_str()].name = aQuery.c_str();

			//store tweets
			for (int i = 0 ; i < tweetsNum; ++i){
				tweets.push_back(doc["statuses"][i]["text"].GetString());	
			}

			AnalyzeCompanySentiment(aQuery,tweets);
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

	out<<"Company,Positive,Neutral,Negative\n";
	//output to screen
	for (map<string,CompanyResults>::iterator it = companyResultsMap.begin(); it != companyResultsMap.end(); ++it)
	{
		CompanyResults companyRes = it->second;
		cout<<it->first.c_str() << " => :\tPos:"<<companyRes.positiveCount<<
										"\tNeut:"<<companyRes.neutralCount<< 
										"\tNeg:"<<companyRes.negativeCount<<"\n\n";

		out<<it->first.c_str()<< ","<<companyRes.positiveCount<<","<<companyRes.neutralCount<<","<<companyRes.negativeCount<<"\n";

	}

	myfile << out.rdbuf();
	myfile.close();

}

void CheckSentimentChange(){

}


int main( int argc, char* argv[] )
{

	//reserve memory to avoid constant resizing
	positiveWords.reserve(5000);
	negativeWords.reserve(5000);
	stopWords.reserve(5000);

	//Load dictionaries from files
	LoadDictionaries();
    
	cout <<"=======================================\n";
	cout<<"Twitter Sentiment Analyzer by Alex Susma\n";
	cout<<"June 2016\n";
	cout <<"=======================================\n\n";

	
	//set username & password
    string userName ="alexsusma";
    string passWord ="Fy6#@A%9";

	//Connect to Twitter
    if (!ConnectToTwitter(userName, passWord, twitterObj)){
		cout<<"Connection failed. Program exiting...";
		return 0;
	}

	//Ask user for input or read the input file based on 
	//the definition of MANUAL_INPUT
	int count = 0;
	vector<string> inputQueries;
	GetInput(inputQueries, count);
	

	int iterations = 0;
	while(1){

		//clock_t start = clock();


#ifdef MULTITHREADING
		//start search and analyse results in separate threads
		thread myThreads[MAX_THREADS];
		for (unsigned int i = 0; i < inputQueries.size();++i){
			myThreads[i] = thread(RunSearchAndAnalysis, inputQueries[i],to_string(count));
		}


		//wait for all threads to finish
		for (unsigned int i = 0; i < inputQueries.size();++i){
			myThreads[i].join();
		}

	//	double diff = (clock() - start) / (double)CLOCKS_PER_SEC;
	//	cout<<"Total time multithreading: "<<diff<<"\n";

#else
		// FOR PERFORMANCE TESTING PURPOSES
	
		//sequential execution 1 thread
		 start = clock();
		for (unsigned int i = 0; i < inputQueries.size();++i){
			RunSearchAndAnalysis(inputQueries[i],to_string(count));
		}

		diff = (clock() - start) / (double)CLOCKS_PER_SEC;
		cout<<"Total time 1 thread: "<<diff<<"\n";

#endif
		if (iterations)
			CheckSentimentChange();
		OutputResults();
		++iterations;
		Sleep(EXECUTION_PERIOD);	
}
	cout<<"Press any key to end the program";
	_getch();
    return 0;
}

