#include <iostream>
#include <fstream>
#include <string>
//#include <cstdint>
//#include <sstream>
//#include <vector>
//#define FMT_HEADER_ONLY

//#include <fmt/format.h>
//#include <fmt/core.h>


#include "read_file.hpp"

using namespace std::filesystem;
using namespace std;


bool read_file(std::string file_name)
{   	
    fstream file;
    file.open(file_name, std::ios::in);
    
    if(!file)
    {
        cout << "Unable to open the file!\n" << endl;
        return false;
    }
    else
        cout << "Succeed to open the file(" << file_name << ")!" << endl;
	
    string line;
    while (getline (file, line))
    {
        cout << "m_deviceId= " << line <<endl;
    }

    file.close();

	return true;
}

