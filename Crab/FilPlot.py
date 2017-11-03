import numpy as np
import scipy as sp
import sys
from sigpyproc.Readers import FilReader
import matplotlib.pyplot as plt

if __name__ == "__main__":

	if(len(sys.argv) < 4):
		print "USAGE : <.fil file> <Start samp>  <Total samp> "
		print "Plot Filterbank file starting from the Start samples with total samples."
		sys.exit(0)

	f	= FilReader(sys.argv[1]);
	f_top  	= f.header['ftop']
	f_bott 	= f.header['fbottom']
	t_rsl	= f.header['tsamp']
	d = f.readBlock(int(sys.argv[2]),int(sys.argv[3]));
	print d.shape,'234234'
	freq    = np.linspace(f_top,f_bott,len(d))
	#np.save('FFT_search/data/'+sys.argv[1][:4]+'_'+sys.argv[2]+'-'+sys.argv[3],d)
	#np.save('FFT_search/data/'+sys.argv[1][:4]+'_freq',freq)
	#np.save('FFT_search/data/crab',d)
	#np.save('FFT_search/data/crab_freq',freq)
	#exit()
	plt.pcolormesh(d)
	#plt.ylim(0,d.shape[0])
	plt.colorbar()
	plt.imshow(d,aspect='auto')
	plt.savefig('DM_sweep.png')
	plt.show()
	
