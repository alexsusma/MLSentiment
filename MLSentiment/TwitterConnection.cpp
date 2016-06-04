#include "TwitterConnection.h"

using namespace std;

bool ConnectToTwitter(string& aUsername, string& aPassword, twitCurl& twitterObj)
{
	
    bool result = false;
	string replyMsg;


	printf("Connecting to Twitter with username: %s ...", aUsername.c_str()); 
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
		cout<<"Connection successful\n";
		result = true;
       // printf( "\ntwitterClient:: twitCurl::accountVerifyCredGet web response:\n%s\n", replyMsg.c_str() );
    }
    else
    {
        twitterObj.getLastCurlError( replyMsg );
        cout<<"\ntwitterClient:: twitCurl::accountVerifyCredGet error:\n"<<replyMsg.c_str()<<"\n";
		result = false;
    }

	return result;
}