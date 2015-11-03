/* -*- c++ -*- */
/* 
 * Copyright 2015 <+YOU OR YOUR COMPANY+>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "csma_impl.h"
#include <boost/crc.hpp>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include "sshm.h"
#include <ctime>





namespace gr {
  namespace ieee802_11 {

    csma::sptr
    csma::make(float threshold, bool debug)
    {
      return gnuradio::get_initial_sptr
        (new csma_impl(threshold, debug));
    }

    /*
     * The private constructor
     */
    csma_impl::csma_impl(float threshold, bool debug)
      : gr::block("csma",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(0, 0, 0))
    {
		message_port_register_out(pmt::mp("out"));
		message_port_register_in(pmt::mp("in"));
		set_msg_handler(pmt::mp("in"),
				boost::bind(&csma_impl::in, this, _1));
		
		d_threshold = threshold;
        d_debug = debug;
		
	}

    /*
     * Our virtual destructor.
     */
    csma_impl::~csma_impl()
    {
    }

    //void
    //csma_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    //{
        //* <+forecast+> e.g. ninput_items_required[0] = noutput_items */
    //}

    //int
    //csma_impl::general_work (int noutput_items,
                       //gr_vector_int &ninput_items,
                       //gr_vector_const_void_star &input_items,
                       //gr_vector_void_star &output_items)
    //{
        //const <+ITYPE*> *in = (const <+ITYPE*> *) input_items[0];
        //<+OTYPE*> *out = (<+OTYPE*> *) output_items[0];

        //// Do <+signal processing+>
        //// Tell runtime system how many input items we consumed on
        //// each input stream.
        //consume_each (noutput_items);

        //// Tell runtime system how many output items we produced.
        //return noutput_items;
    //}
    
    void
    csma_impl::in(pmt::pmt_t msg)
    {
	
	    
		// get share memory
		int segmentid;
		double * power;
		segmentid=shm_get(123456,(void**)&power,sizeof(double));
		if (segmentid<0){printf("Error creating segmentid"); exit(0);};
		
		//parameter declaration
		int max_attempts = 4;
		int cwmin[2] = {3, 15};
		double slot_time = 13.0; //micro-seconds
		double aifs[2] = {58.0, 149.0};
		
		// extract the dictionary for the ac level
		pmt::pmt_t p_dict(pmt::car(msg));
		pmt::pmt_t ac_level = pmt::dict_ref(p_dict, pmt::mp("ac_level"), pmt::PMT_NIL);
		int ac = (int) pmt::to_long(ac_level);
		
		
		
		if (d_debug)
		{	
			fp.open("csma_log.txt");
			fp << "==========msg received===========\n";
			fp << "ac level: " << ac <<std::endl;
	    }
		
		
		//check channel state
		bool okay_to_send = false;
		okay_to_send = channel_state(d_threshold, power); 							// need to deal with
		if (d_debug)
		{
			if (okay_to_send) {fp << "channel is free, jumping to send...\n";}
			else {fp << "channel is BUSY, waiting...\n";}
		}
		
		int n_attempts = 0;
		double counter;
		int backoff;
		
		
		
		//state machine
		
		while (!okay_to_send)
		{
			if (d_debug) fp << "waiting AIFS "<<aifs[ac] << "micro secs\n";
			//wait for aifs
			wait_time(aifs[ac]);
			
			
			//slot
			srand(time(NULL));
			backoff = rand() % (int)(cwmin[ac]*pow(2,n_attempts));
			counter = backoff;
		
			if (d_debug) fp << "attempt #"<<n_attempts<<", random backoff: "<<backoff <<", counter: "<<counter<<std::endl;
		
			while (counter>0) 
			{
				wait_time(slot_time);
				
				counter--;
				
				if (d_debug) fp << "attempt #"<<n_attempts<<", random backoff: "<<backoff <<", counter: "<<std::endl;
				
				okay_to_send = channel_state(d_threshold, power);
				if (okay_to_send) {fp << "channel is free\n" ;}
				else {fp << "channel is busy, back to aifs\n";}
				if (!okay_to_send)
				{
					wait_time(aifs[ac]);
				}
			}
			
			okay_to_send = channel_state(d_threshold, power);
			n_attempts++;
			if (n_attempts > max_attempts) {
				if (d_debug) {fp << "max attempts reached\n";}
				return;
				}
			
		}
		
		//send the msg
		
		message_port_pub(pmt::mp("out"), msg);
		if (d_debug) fp << "msg sent. Waiting for aifs\n";
		//post-tx aifs
		
		wait_time(aifs[ac]);
		
		if (d_debug) {fp.close();}
		
	}
	
	void 
	csma_impl::wait_time(double wait_duration)
	{
		time_t start_time;
		time_t stop_time;
		
		time(&start_time);
		time(&stop_time);
		while((stop_time - start_time)/1000000 <= wait_duration)
		{
			
			time(&stop_time);
		}
	}
	
	bool 
	csma_impl::channel_state(float threshold, double * power)
	{
		
		if (*power >= threshold) {return true;}
		return false;
	}

  } /* namespace ieee802_11 */
} /* namespace gr */

