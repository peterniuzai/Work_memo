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

def mydatarec(strt_time,volt_agg):
	fpga.write_int('adc_scope1_snapshot_ctrl',0)
	chl_a=fpga.snapshot_get('adc_scope0_snapshot', man_trig=True, man_valid=True)
	fpga.write_int('adc_scope1_snapshot_ctrl',1)
	chl_b=fpga.snapshot_get('adc_scope1_snapshot', man_trig=True, man_valid=True)

	value_a=numpy.fromstring(chl_a['data'], dtype = numpy.int8)
	inter_analog_a=[]
	value_b=numpy.fromstring(chl_b['data'], dtype = numpy.int8)
	inter_analog_b=[]

	for iana in range(len(value_a)):
		inter_analog_a.append(value_a[iana])
	iana=0
        for iana in range(len(value_b)):
		inter_analog_b.append(value_b[iana])

        volt_agg.append(inter_analog_a)
        
        mydatarec(strt_time,volt_agg)
        
if __name__ == '__main__':

	print 'Connecting to board:', HOST
	fpga = corr.katcp_wrapper.FpgaClient(HOST)
	time.sleep(0.1)

	if fpga.is_connected():
		print 'DONE'
	else:
		print 'ERROR: Failed to connect to server %s!' %(HOST)
		sys.exit(0);

        # get all the snapshot volts
        volt_agg = []
        
	# start the process
	print 'Starting data recording...'
        strt_time = time.clock()
	mydatarec(strt_time,volt_agg)

        except KeyboardInterrupt:
                print "Interrupt from keyboard"
        except:
                print "Other exception"
        finally:
                with open('adc_monitor.bin','w') as fp:
                        np.save(fp,volt_agg)
