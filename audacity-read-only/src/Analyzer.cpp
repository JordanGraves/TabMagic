#include "Analyzer.h"


#include "Project.h"
#include "WaveTrack.h"
#include "Prefs.h"
#include "Spectrum.h"
#include "FFT.h"
#include <math.h>
#include "PitchName.h"


/**
 * 
 *  Cubic Interpolate Helper
 */
float myCubicInterpolate(float y0, float y1, float y2, float y3, float x)
{
   float a, b, c, d;

   a = y0 / -6.0 + y1 / 2.0 - y2 / 2.0 + y3 / 6.0;
   b = y0 - 5.0 * y1 / 2.0 + 2.0 * y2 - y3 / 2.0;
   c = -11.0 * y0 / 6.0 + 3.0 * y1 - 3.0 * y2 / 2.0 + y3 / 3.0;
   d = y0;

   float xx = x * x;
   float xxx = xx * x;

   return (a * xxx + b * xx + c * x + d);
}

/**
 *  
 *   SampleCount to Time
 * 
 */
float SampleCountToTime(sampleCount samples, sampleCount total, float length_in_sec, int samplerate) {
      return (1.0 * samples) / (1.0 *samplerate);
}


Analyzer::Analyzer(int tempo, int samplerate, int dBrange, Track* t) {
      cout << "Constructing Analyzer" << endl;;
      _tempo = tempo;
      _samplerate = samplerate;
      _dBrange = dBrange;
      _t = t;
      
      _f.open("Analyzed.txt");
}

int Analyzer::process() {
    cout << "Process() " << endl;
    // Get total time of track
    double length_in_sec = _t->GetEndTime() - _t->GetStartTime();
    WaveTrack *track = (WaveTrack *)_t;		//There's a better way to handle this
    AudacityProject *p = GetActiveProject();    
   
    // convert to samples
    sampleCount start, end, intervalEnd, intervalSize;
    start = track->TimeToLongSamples(p->mViewInfo.sel0); //end = track->TimeToLongSamples(p->mViewInfo.sel1);
    end = length_in_sec * _samplerate;
    
    double block_len_in_sec = 60.0/(double)_tempo;
    intervalSize = block_len_in_sec * _samplerate;
    float startTime = 0.0;
    int count = 0;	// just for checking loop
			    cout << "Start: "<< start << "   End: " << end << endl << endl;
    while(start <= end) {
			    cout << "Block: " << count << endl;
	  // Get end of this interval in samples
	  intervalEnd = (startTime + block_len_in_sec) * _samplerate;
	  int numSamples = (sampleCount)(intervalEnd - start);
			    cout << "Number of Samples: " << numSamples << endl;
	  float *sampleDataBuffer1 = new float[numSamples];
	  // Get sample info into array
	  track->Get((samplePtr)sampleDataBuffer1, floatSample, start, numSamples);
	  float *sampleData = new float[numSamples];
	  for(int i=0; i< numSamples; i++)		// What would happen if we skipped this and just used the first buffer??
		sampleDataBuffer1[i] += sampleData[i];
	  delete[] sampleDataBuffer1;
	  
	  // Now we enter our analysis of the samples..enter black hole ->
	  
	  int alg = wxAtoi( gPrefs->Read(wxT("/FreqWindow/AlgChoice"), wxT("")) );
	  int func = wxAtoi( gPrefs->Read(wxT("/FreqWindow/FuncChoice"), wxT("")) );
	  long mWindowSize = 4096;	// This value is ok for the frequency range we are working in
	  int half = mWindowSize / 2;	// Why we get half?	  
	  float *out = new float[mWindowSize];		// Surely we can get rid of some of this...after I figure out what the hell it's all for
	  float *in = new float[mWindowSize];
	  float *win = new float[mWindowSize];
	  float *mProcessed = new float[mWindowSize];
	  // Recalc()
	  for(int i=0; i<mWindowSize; i++)
		win[i] = 1.0;
	  WindowFunc(func, mWindowSize, win);
	  double wss = 0;
	  for(int i=0; i<mWindowSize; i++)
		wss += win[i];
	  if(wss > 0)
		wss = 4.0 / (wss*wss);
	  else
		wss = 1.0;      
	  int jstart = 0;		// What these are for...??
	  int jwindows = 0;
	  int i;
	  while (jstart + mWindowSize <= numSamples) {
		for (i = 0; i < mWindowSize; i++)
		      in[i] = win[i] * sampleData[jstart + i];
		PowerSpectrum(mWindowSize, in, out);
		for (i = 0; i < half; i++)
		      mProcessed[i] += out[i];       
		jstart += half;
		jwindows++;
	  }
	  delete[] sampleData;
	  // Convert to decibels
	  double scale;
	  float mYMin = 1000000.;
	  float mYMax = -1000000.;
	  int mProcessedSize;
	  float mYStep;
	  scale = wss / (double)jwindows;
	  for (int i = 0; i < half; i++)
	  {
		mProcessed[i] = 10 * log10(mProcessed[i] * scale);
		if(mProcessed[i] > mYMax)
		      mYMax = mProcessed[i];
		else if(mProcessed[i] < mYMin)				// What the f*** is going on???
		      mYMin = mProcessed[i];
	  }
	  if(mYMin < -_dBrange)
		mYMin = -_dBrange;
	  if(mYMax <= -_dBrange)
		mYMax = -_dBrange + 10.; // it's all out of range, but show a scale.
	  else
		mYMax += .5;
	  mProcessedSize = half;
	  mYStep = 10;
	  
	  //From DrawPlot();
	  float yTotal = (mYMax - mYMin);
	  float xMin, xMax, xRatio, xStep;
	  int iwidth = 656;	// Hardcoded
	  xMin = _samplerate / mWindowSize;
	  xMax = _samplerate / 2;
	  xRatio = xMax / xMin;
	  xStep = pow(2.0f, (log(xRatio) / log(2.0f)) / iwidth);
	  float xPos = xMin;
	  int loudest_freq = 0;
	  int highest_db = -10000;
	  float value;
	  for (i = 0; i < iwidth; i++) {
	      float y;
	      /** Get Processed Value() **/
	      float freq0 = xPos;
	      float freq1 = xPos * xStep;
	      float bin0, bin1, binwidth;
	      bin0 = freq0 * mWindowSize / _samplerate;
	      bin1 = freq1 * mWindowSize / _samplerate;
	      binwidth = bin1 - bin0;
	      value = float(0.0);
	      if (binwidth < 1.0) {
		  float binmid = (bin0 + bin1) / 2.0;
		  int ibin = int (binmid) - 1;
		  if (ibin < 1)
		    ibin = 1;
		  if (ibin >= mProcessedSize - 3)
		    ibin = mProcessedSize - 4;
		  value = myCubicInterpolate(mProcessed[ibin],
				  mProcessed[ibin + 1],
				  mProcessed[ibin + 2],
				  mProcessed[ibin + 3], binmid - ibin);
	      } 
	      else {
		  if (int (bin1) > int (bin0))
		    value += mProcessed[int (bin0)] * (int (bin0) + 1 - bin0);
		  bin0 = 1 + int (bin0);
		  while (bin0 < int (bin1)) {
		    value += mProcessed[int (bin0)];
		    bin0 += 1.0;
		  }
		  value += mProcessed[int (bin1)] * (bin1 - int (bin1));
		  value /= binwidth;
	      }
	      y = value;
	      /** End Get Processed Value **/
	      /* ------ MY CODE FOR HIGHEST FREQ ----*/
	      if (y > highest_db) {
		  loudest_freq = xPos;
		  highest_db = y;
	      }
	      xPos *= xStep;
	  }
	  delete[] out;		
	  delete[] in;
	  delete[] win;
	  delete[] mProcessed;
	  
	  wxString xpitch;
	  xpitch = PitchName_Absolute(FreqToMIDInote(loudest_freq));
	
	  string note = std::string(xpitch.mb_str());
	  
	  // Write to File
	  _f << "Start Time: " << SampleCountToTime(start, numSamples, length_in_sec, _samplerate) << endl
	     <<	"EndTime: " << SampleCountToTime(start + intervalSize, numSamples, length_in_sec, _samplerate) << endl 
	     << "Note: " << note << endl
	     << "dB: " << highest_db << endl;
	  
	  
	  
	  startTime += block_len_in_sec;
	  start += intervalSize;
	  count++;
    }
    _f.close();
    cout << count << endl;
    return 0;  
}
