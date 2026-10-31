#pragma once
#include <string>
using namespace std;

class InputClosed {};   

string readLine(const string& prompt);                            
int readInt(const string& prompt, int minValue, int maxValue);     
double readDouble(const string& prompt);
string readPassword(const string& prompt);                         
