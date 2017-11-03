#! /usr/bin/python
import corr,time,numpy,struct,sys,logging,pylab
from optparse import OptionParser
from numpy import int32,uint32,array,zeros,arange
import matplotlib.pyplot as plt
import matplotlib.mlab as mlab
from numpy import mean,sqrt,square,arange

HOST = 'rpicrab'

def exit_fail():
	print 'FAILURE DETECTED. Log entries:\n',lh.printMessages()
	try:
		fpga.stop()
	except: pass
	raise
	exit()

def exit_clean():
	try:
		fpga.stop()
	except: pass
	exit()

def myplot():
	plt.clf()
	fpga.write_int('adc_scope1_snapshot_ctrl',0)
	chl_a=fpga.snapshot_get('adc_scope0_snapshot', man_trig=True, man_valid=True)
	fpga.write_int('adc_scope1_snapshot_ctrl',1)
	chl_b=fpga.snapshot_get('adc_scope1_snapshot', man_trig=True, man_valid=True)
	#a=fpga.get_adc_snapshots('snapshot0')
	#file=open('li0604.bin','wb')
	#value_a=struct.unpack('<8192B', chl_a[0:])
	#rms=sqrt(mean(square(chl_a))
	#print rms
	value_a=numpy.fromstring(chl_a['data'], dtype = numpy.int8)
	inter_analog_a=[]
	value_b=numpy.fromstring(chl_b['data'], dtype = numpy.int8)
	inter_analog_b=[]
	# = struct.unpack('<512H',value_a[0::2]))

	'''xy_img_tmp = data1[0::4]
	xy_real_tmp = data1[1::4]
	yy_tmp = data1[2::4]
	xx_tmp = data1[3::4]'''
	#print len(value_a)
	for iana in range(len(value_a)):
		inter_analog_a.append(value_a[iana])
	iana=0
	for iana in range(len(value_b)):
		inter_analog_b.append(value_b[iana])
	#inter_analog=b-128
	#s=[[1,5,6,5,8,4,2,1,5,4,1,1,9,10,8,6]]
	#mu,sigma=2,0.5
	#s=numpy.random.normal(mu,sigma,1000)
	#print "%d"%(s)
	#file.write(b)
	#file.close()
# create ideal gaussian shape for 8 bits
	th_id = 32 # ideal thresh
	bins8=arange(-130.5,130.5,1)
	g =mlab.normpdf(bins8,0,th_id)
	plt.subplot(3,2,1)
	plt.xticks(range(-130, 131, 40))
	#histData, bins, patches=plt.hist(b, bins = 256, range = (-128,128))
	plt.ylabel('Probability')
	#plt.ylim(0,1000)
	plt.xlabel('ADC sample bins.')
	plt.title('Histogram Pol1',bbox=dict(facecolor='red', alpha=0.5))
	#plt.hist(inter_analog, bins = 256, range = (-128,128))
	plt.hist(inter_analog_a, bins8,normed=1,facecolor='blue',alpha=0.9,histtype='stepfilled')
	plt.plot(bins8,g,'red',linewidth=1)

	plt.subplot(3,2,2)
	plt.xticks(range(-130, 131, 40))
	plt.ylabel('Probability')
	#plt.ylim(0,1000)
	plt.xlabel('ADC sample bins.')
	plt.title('Histogram Pol2',bbox=dict(facecolor='red', alpha=0.5))
	#plt.hist(inter_analog, bins = 256, range = (-128,128))
	plt.hist(inter_analog_b, bins8,normed=1,facecolor='blue',alpha=0.9,histtype='stepfilled')
	plt.plot(bins8,g,'red',linewidth=1)
 

	plt.subplot(3,2,3)
	plt.title('Pol1 Time-domain ')
	plt.ylabel('Level')
	plt.xlabel('Time (3.8 nanosec)')
	#plt.set_yticks(range(-130, 131, 10))
	plt.xlim(0,1000)
	plt.plot(inter_analog_a,'b') 

	plt.subplot(3,2,4)
	plt.title('Pol2 Time-domain ')
	#plt.set_ylim(-max_lev-1,max_lev+1)
	plt.ylabel('Level')
	plt.xlabel('Time (3.8 nanosec)')
	plt.xlim(0,500)
	plt.plot(inter_analog_b,'r')

	n_chans=256
	n_accs=len(inter_analog_a)/n_chans/2
	freqs=numpy.arange(n_chans/2,n_chans,0.5)*float(1050*2)/n_chans #channel center freqs in Hz.
	window=numpy.hamming(n_chans*2)
	spectrum_a=numpy.zeros(n_chans)
	for acc in range(n_accs):
		spectrum_a += numpy.abs((numpy.fft.rfft(inter_analog_a[n_chans*2*acc:n_chans*2*(acc+1)]*window)[0:n_chans])) 
	#print spectrum.shape
	#print spectrum
	spectrum_a  = 20*numpy.log10(spectrum_a/n_accs/n_chans*4.91)-60
	#print spectrum.shape
	#print spectrum
	#print 'plotting from %i to %i'%(t_start,max_pos-1)
	spectrum_a = spectrum_a[::-1]
	pylab.hold(True)
	#plt.plot(freqs/1e6,fullSpectrum,label='Signal on')
	pylab.hold(True)
	plt.subplot(3,2,5)
	plt.plot(freqs,spectrum_a)
	#plt.legend()
	plt.title('Spectrum of capture (%i samples)'%(len(value_a)))
	plt.ylabel('Power (dBm)')
	plt.xlabel('Frequency (MHz)')


	n_accs=len(inter_analog_b)/n_chans/2
#	freqs=numpy.arange(n_chans)*float(1150000000)/n_chans #channel center freqs in Hz.
	window=numpy.hamming(n_chans*2)
	spectrum_b=numpy.zeros(n_chans)
	for acc in range(n_accs):
		spectrum_b += numpy.abs((numpy.fft.rfft(inter_analog_b[n_chans*2*acc:n_chans*2*(acc+1)]*window)[0:n_chans])) 
	#print spectrum.shape
	#print spectrum
	spectrum_b  = 20*numpy.log10(spectrum_b/n_accs/n_chans*4.91+0.0000001)-60
	#print spectrum.shape
	#print spectrum
	#print 'plotting from %i to %i'%(t_start,max_pos-1)
	spectrum_b = spectrum_b[::-1]
	pylab.hold(True)
	#plt.plot(freqs/1e6,fullSpectrum,label='Signal on')
	pylab.hold(True)
	plt.subplot(3,2,6)
	plt.plot(freqs,spectrum_a,'b')
	plt.plot(freqs,spectrum_b,'r')
	#plt.legend()
	plt.title('Spectrum of capture (%i samples)'%(len(value_b)))
	plt.ylabel('Power (dBm)')
	plt.xlabel('Frequency (MHz)')

	fig.canvas.draw()
	fig.canvas.manager.window.after(100, myplot)

if __name__ == '__main__':

	print 'Connecting to board:', HOST
	fpga = corr.katcp_wrapper.FpgaClient(HOST)
	time.sleep(0.1)

	if fpga.is_connected():
		print 'DONE'
	else:
		print 'ERROR: Failed to connect to server %s!' %(HOST)
		sys.exit(0);

	# set up the figure with a subplot for each polarisation to be plotted
	fig = plt.figure()

	# create the subplots
	subplots = []
	for p in range(3):
		subPlot = fig.add_subplot(3, 1, p + 1)
		subplots.append(subPlot)
	# start the process
	print 'Starting plots...'
	fig.subplots_adjust(hspace=0.8)
	fig.canvas.manager.window.after(100, myplot)
	#plt.set_title('Histogram as at')
	plt.show()
