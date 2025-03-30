#include "app.h"

//this function is called once at startup
void apptaskInitHook( void )
{
	//TODO add here application initialization code 
}

//this function is called once every 1 ms
int apptask1msHook( void )
{
	int retVal = 0;




	//TODO replace the test code below (used to generate some load) with app code
	unsigned int sum = 0;

	for (retVal = 1000; retVal>0; retVal--)
	{
		sum += retVal;
	}

	//end of test code




	return retVal;
}

void apptaskbgHook(void)
{

}