
#ifndef __AUDACITY_BLOCK__
#define __AUDACTITY_BLOCK__

#include <iostream>
using namespace std;
class Block {
  public:
	float _note;
	float _frequency;
	float _dB;
  
  Block() {
    
  }
  Block(float frequency, float dB) {
     cout << "Creating new block w/ freq: " << frequency << "  dB: " << dB << endl;
     _frequency = frequency;
     _dB = dB;
  }
  
  
};

#endif