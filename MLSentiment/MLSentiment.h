#include <vector>
#include <string>
#include <map>
#include "include/twitcurl.h"

using namespace std;


struct CompanyResults{
	vector<int> sentimentsPerTweet;
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

std::vector<string> positiveWords;
std::vector<string> negativeWords;
std::vector<string> stopWords;
twitCurl twitterObj;
std::map<string,CompanyResults> companyResultsMap;