// #include <boost/math/distributions/binomial.hpp>
// #include <boost/math/distributions/negative_binomial.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

#include "dynaplex/dynaplexprovider.h"
using namespace DynaPlex;
using namespace std;


const int machine_col = 2;
const int mttf_col = 4;
const int lead_time_col = 3;
const int a_col = 5;
const int bsp_n_col = 8;
const int probsp_n_col = 10;
const int probsp_xo_col = 11;

void readdata() 
{
	ifstream file("all_data.csv");
    if (!file.is_open()) {
        throw DynaPlex::Error("File open");
    }

    std::string line, word;
    std::vector<std::vector<std::string>> data;
    
    // Read each line from the file
    while (std::getline(file, line)) 
    {
        std::stringstream ss(line);
        std::vector<std::string> row;
        
        // Split the line by commas
        while (std::getline(ss, word, ',')) {
            row.push_back(word);  // Store each word in the row
        }
        
        data.push_back(row);
    }

    file.close();  // Close the file

    // Display the data (for testing purposes)
    for (const auto& row : data) {
        std::cout<< row[machine_col] << std::endl;
    }

}