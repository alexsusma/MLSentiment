#include <vector>
#include <string>
#include <map>
#include "include/twitcurl.h"

using namespace std;


struct CompanyResults{
	vector<int> sentimentsPerTweet;
	vector<string> tweets;
	int negativeCount;
	int neutralCount;
	int positiveCount;
	string name;
	bool hasPrevResults;

	CompanyResults()
		:negativeCount(0)
		,neutralCount(0)
		,positiveCount(0)
		,hasPrevResults(false)
		,name("")
	{
	}

};

vector<string> positiveWords;
vector<string> negativeWords;
vector<string> stopWords;
twitCurl twitterObj;
map<string,CompanyResults> companyResultsMap;