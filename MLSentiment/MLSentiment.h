#include <vector>
#include <string>
#include <map>
#include "include/twitcurl.h"

using namespace std;

enum SentimentStatus{
	ssNegative,
	ssNeutral,
	ssPositive
};


struct Sentiment{
	int count;


	Sentiment()
		:count(0)
	{
	}
};

//structure which will hold results for 
//one particular company
struct CompanyResults{
	vector<int> sentimentsPerTweet;
	vector<string> tweets;
	Sentiment positive;
	Sentiment negative;
	Sentiment neutral;
	string name;
	SentimentStatus status;
	bool statusChanged;
	bool hasPrevResults;

	CompanyResults()
		:hasPrevResults(false)
		,name("")
		,status(ssNeutral)
		,statusChanged(false)
	{
	}

};

//dictionary of positive words
vector<string> positiveWords;

//dictionary of negative words
vector<string> negativeWords;

//dictionary of stop words
vector<string> stopWords;

//twitCurl object used to access Twitter
twitCurl twitterObj;

int tweetsCount;

//map of company results.
//Key = Company name
//Value = CompanyResults object 
map<string,CompanyResults> companyResultsMap;