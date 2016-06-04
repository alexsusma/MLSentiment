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



 #define MAX_THREADS 10

using namespace rapidjson;
using namespace std;




void GetInput(vector<string>& aQueries, int& aCount)
{
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

}

/*LoadDictionaries will read in 3 text files and store the contents in 3 string vectors.
* The vectors will contain english positive, negative and stop words.
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
}


/*SentimentToString will take an integer sentiment value and return the string associated with it.
* Possible returns: "neutral","positive","negative"
*/
string SentimentToString(int sentiment)
{
	string result = "neutral";
	if (sentiment < 0)
		result = "negative";
	else if (sentiment > 0)
		result = "positive";

	return result;
}

/*SplitTweet will take a string containing a tweet and break it down into words based on the delimiter.
* Return value is a vector of strings which contains the tweet words.
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



void ProcessTweet(string& tweet)
{
	//remove hashtags

	size_t position = 0;
	size_t start;
	while(position != string::npos) {
		start = tweet.find_first_of("+\"%!()[]{}:;<>/\\=^&*~#?$1234567890", position);
		if (start != string::npos)
			tweet.replace(start,1,"");
		else
			break;
		if(position != string::npos)
			position++;
	}
	
	//remove RT
	position = 0;
	start = 0;
	while(position != string::npos) {
		start = tweet.find("RT", position);
		if (start != string::npos)
			tweet.replace(start,2,"");
		else
			break;
		if(position != string::npos)
			position++;
	}

	//make lowercase
	for (std::string::size_type i=0; i<tweet.length(); ++i)
		tweet.at(i) = tolower(tweet[i]);
	
	//remove @username
	position = 0;
	start = 0;
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

	//remove url
	position = 0;
	start = 0;
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
}

void AnalyzeCompanySentiment(string& aCompanyName, vector<string>& tweets)
{
	CompanyResults& myResults = companyResultsMap[aCompanyName.c_str()];

	for (vector<string>::iterator it = tweets.begin(); it != tweets.end(); ++it)
	{
		string tweet = *it;
		ProcessTweet(tweet);
		int sentiment = AnalyzeTweetSentiment(tweet);				
		string sentimentStr = SentimentToString(sentiment);
				
				 
		myResults.sentimentsPerTweet.push_back(sentiment);

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
	myResults.overallSentiment = max(myResults.positiveCount,max(myResults.negativeCount,myResults.neutralCount));
				
}




void RunSearchAndAnalysis(string& aQuery, string aCount)
{

	string replyMsg;
	vector<string> tweets;
	twitCurl* twit = twitterObj.clone();

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

void OutputResults()
{
	for (map<string,CompanyResults>::iterator it = companyResultsMap.begin(); it != companyResultsMap.end(); ++it)
	{
		CompanyResults companyRes = it->second;
		cout<<it->first.c_str() << " has the following sentiment counts:\tPos:"<<companyRes.positiveCount<<
																		"\tNeut:"<<companyRes.neutralCount<< 
																		"\tNeg:"<<companyRes.negativeCount<<"\n\n";

	}
}
int main( int argc, char* argv[] )
{

	//reserve memory to avoid constant resizing
	positiveWords.reserve(5000);
	negativeWords.reserve(5000);
	stopWords.reserve(5000);

	//Load dictionary from files
	LoadDictionaries();

	//set username & password
    string userName ="alexsusma";
    string passWord ="Fy6#@A%9";
    
	cout <<"========";
	cout<<"Twitter Sentiment Analyzer by Alex Susma\n";
	cout<<"June 2016\n";
	cout <<"========\n\n";

	//Connect to Twitter
    if (!ConnectToTwitter(userName, passWord, twitterObj)){
		cout<<"Connection failed. Program exiting...";
		return 0;
	}

	//Ask user for input
	int count = 0;
	vector<string> inputQueries;
	GetInput(inputQueries, count);
	
	//start search and analyse results
	thread myThreads[MAX_THREADS];
	for (unsigned int i = 0; i < inputQueries.size();++i){
		myThreads[i] = thread(RunSearchAndAnalysis, inputQueries[i],to_string(count));
	}

	for (unsigned int i = 0; i < inputQueries.size();++i){
		myThreads[i].join();
	}

	OutputResults();

	cout<<"Press any key to end the program";
	_getch();
    return 0;
}

