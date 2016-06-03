#include <vector>
#include <string>
#include <algorithm>
#include "MLSentiment.h"
#include "document.h"
#include "writer.h"
#include "stringbuffer.h"
#include <iostream>
#include <fstream>
#include <string.h>
#include "pointer.h"
#include <thread>
#include <conio.h>
#include "include/twitcurl.h"


 #define MAX_THREADS 10

using namespace rapidjson;
using namespace std;

struct CompanyResults{
	vector<int> sentimentsPerTweet;
	int absoluteSentiment;
	int negativeCount;
	int neutralCount;
	int positiveCount;
	int overallSentiment;
	string name;

	CompanyResults()
		:absoluteSentiment(0)
		,negativeCount(0)
		,neutralCount(0)
		,positiveCount(0)
		,overallSentiment(0)
	{
	}

};

std::vector<string> positiveWords;
std::vector<string> negativeWords;
std::vector<string> stopWords;
twitCurl twitterObj;
std::map<string,CompanyResults> companyResultsMap;

void GetInput(vector<string>& aQueries, int& aCount)
{
	char tmpBuf[1024];
	printf( "\nEnter companies to research separated by comma(i.e. Apple,Google,Netflix...):\n\n" );
    memset( tmpBuf, 0, 1024 );
	cin.getline(tmpBuf,sizeof(tmpBuf));
	
	//parse the input into individual company names and store them
	char * pch;
	char * context;
     pch = strtok_s (tmpBuf,",",&context);
	while (pch != NULL)
	{
		aQueries.push_back(string(pch));
		pch = strtok_s (NULL, ",",&context);
	}

	printf( "\nHow many tweets to analyze per company ?\n\n" );
	cin >> aCount;

}

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

string SentimentToString(int sentiment)
{
	string result = "neutral";
	if (sentiment < 0)
		result = "negative";
	else if (sentiment > 0)
		result = "positive";

	return result;
}

vector<string> splitTweet(const string &tweet, char delimiter) {
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
	vector<string> words = splitTweet(tweet,' ');
				
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
		myResults.absoluteSentiment+=sentiment;
			
		//printf("Sentiment %s:   %s\n\n\n\n", sentimentStr.c_str(), tweet );
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
	//printf("Company %s has neg %d ,neut %d ,pos %d:   \n\n\n",aCompanyName.c_str(), 
	//		myResults.negativeCount, myResults.neutralCount, myResults.positiveCount );
	
	
				
}


bool ConnectToTwitter(string& aUsername, string& aPassword, twitCurl& twitterObj)
{
	
    bool result = false;
	string replyMsg;


	printf("Connecting to Twitter with username: %s \n", aUsername.c_str()); 
    /* Set twitter username and password */
    twitterObj.setTwitterUsername( aUsername );
    twitterObj.setTwitterPassword( aPassword );

   
    /* OAuth flow begins */
    /* Step 0: Set OAuth related params. These are got by registering your app at twitter.com */
    twitterObj.getOAuth().setConsumerKey( std::string( "eFOtYIhgDxdL8C7q5cqBeCfit" ) );
    twitterObj.getOAuth().setConsumerSecret( std::string( "hWqg9GUT849yUPh0J6qtct8xK38pcbMC6fbWJggTmCujlGotLl" ) );

    /* Check if we alredy have OAuth access token from a previous run */
    string myOAuthAccessTokenKey("");
    string myOAuthAccessTokenSecret("");
    
    myOAuthAccessTokenKey = "23764909-HI1iGuKgDVYOptVhqmkKqQIS67kplGLYUnS3gigKz";
	myOAuthAccessTokenSecret = "M7WU0pDhxhx7fuw5hNTmEplQtsLilzPdgZLrqwCNXqSOv";

    
    if( myOAuthAccessTokenKey.size() && myOAuthAccessTokenSecret.size() )
    {
        twitterObj.getOAuth().setOAuthTokenKey( myOAuthAccessTokenKey );
        twitterObj.getOAuth().setOAuthTokenSecret( myOAuthAccessTokenSecret );
    }
    
    /* OAuth flow ends */

    /* Account credentials verification */
    if( twitterObj.accountVerifyCredGet() )
    {
        twitterObj.getLastWebResponse( replyMsg );
		printf ("Connection sucessful");
		result = true;
       // printf( "\ntwitterClient:: twitCurl::accountVerifyCredGet web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        printf( "\ntwitterClient:: twitCurl::accountVerifyCredGet error:\n%s\n", replyMsg.c_str() );
		result = false;
    }

	return result;
}


void RunSearchAndAnalysis(string& aQuery, string aCount)
{

	string replyMsg;
	vector<string> tweets;
	twitCurl* twit = twitterObj.clone();

    if( twit->search( aQuery,  aCount ) )
    {
        twit->getLastWebResponse( replyMsg );
       // printf( "\ntwitterClient:: twitCurl::search web response:\n%s\n\n", replyMsg.c_str() );
	
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
        printf( "\ntwitterClient:: twitCurl::search error:\n%s\n", replyMsg.c_str() );
    }
}

void OutputResults()
{
	for (map<string,CompanyResults>::iterator it = companyResultsMap.begin(); it != companyResultsMap.end(); ++it)
	{
		CompanyResults companyRes = it->second;
		printf("Company %s has the following sentiment counts: Pos: %d, Neut: %d, Neg: %d\n\n",
			it->first.c_str(),companyRes.positiveCount,companyRes.neutralCount,companyRes.negativeCount);
	}
}
int main( int argc, char* argv[] )
{
	positiveWords.reserve(5000);
	negativeWords.reserve(5000);

	//Load dictionary from files
	LoadDictionaries();

	//set username & password
    string userName ="alexsusma";
    string passWord ="Fy6#@A%9";
    
	//Connect to Twitter
    if (!ConnectToTwitter(userName, passWord, twitterObj)){
		printf("Connection failed");
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

	printf("Press any key to end the program");
	_getch();
    return 0;
}

