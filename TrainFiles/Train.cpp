//  - Trains and Roller Coasters
// Sample Code - written by Michael Gleicher
//
// Note: this code is meant to serve as both the example solution, as well
// as the starting point for student projects. Therefore, some (questionable)
// design tradeoffs have been made - please see the ReadMe-Initial.txt file
//

// file Train.cpp - the "main file" and entry point for the train project
// this basically opens up the window and starts up the world

#include "stdio.h"

#include "TrainWindow.H"

#pragma warning(push)
#pragma warning(disable:4312)
#pragma warning(disable:4311)
#include <Fl/Fl.h>
#pragma warning(pop)


int main(int, char**)
{
	printf("CSC3260 Train Project\n");

	TrainWindow tw;
	tw.show();

	Fl::run();
}

// $Header: /p/course/-gleicher/private/CVS/TrainFiles/Train.cpp,v 1.2 gleicher Exp $
