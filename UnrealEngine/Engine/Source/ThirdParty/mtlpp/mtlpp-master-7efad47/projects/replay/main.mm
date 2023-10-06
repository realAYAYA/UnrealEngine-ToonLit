//
//  main.cpp
//  replay
//
//  Created by Mark Satterthwaite on 3/13/19.
//  Copyright © 2019 Nikolay Aleksiev. All rights reserved.
//

#include <iostream>
#include <string>
#include "MTITrace.hpp"

int main(int argc, const char * argv[]) {
	// insert code here...
	// std::cout << "Hello, World!\n";
	
	MTITrace::Get().Replay("/Users/marksatt/Documents/TP_SideScrollerBP_Metal2.mtlpptrace");
	return 0;
}
