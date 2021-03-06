/*!
 * \file gnss_sdr_sample_counter.cc
 * \brief Simple block to report the current receiver time based on the output of the tracking or telemetry blocks
 * \author Javier Arribas 2018. jarribas(at)cttc.es
 *
 *
 * -----------------------------------------------------------------------------
 *
 * GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
 * This file is part of GNSS-SDR.
 *
 * Copyright (C) 2010-2020  (see AUTHORS file for a list of contributors)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * -----------------------------------------------------------------------------
 */

#include "gnss_sdr_sample_counter.h"
#include "gnss_synchro.h"
#include <gnuradio/io_signature.h>
#include <pmt/pmt.h>        // for from_double
#include <pmt/pmt_sugar.h>  // for mp
#include <cmath>            // for round
#include <iostream>         // for operator<<
#include <string>           // for string

gnss_sdr_sample_counter::gnss_sdr_sample_counter(
    double _fs,
    int32_t _interval_ms,
    size_t _size) : gr::sync_decimator("sample_counter",
                        gr::io_signature::make(1, 1, _size),
                        gr::io_signature::make(1, 1, sizeof(Gnss_Synchro)),
                        static_cast<uint32_t>(std::round(_fs * static_cast<double>(_interval_ms) / 1e3)))
{
    message_port_register_out(pmt::mp("sample_counter"));
    set_max_noutput_items(1);
    interval_ms = _interval_ms;
    fs = _fs;
    samples_per_output = std::round(fs * static_cast<double>(interval_ms) / 1e3);
    sample_counter = 0;
    current_T_rx_ms = 0;
    current_s = 0;
    current_m = 0;
    current_h = 0;
    current_days = 0;
    report_interval_ms = 1000;     // default reporting 1 second
    flag_enable_send_msg = false;  // enable it for reporting time with asynchronous message
    flag_m = false;
    flag_h = false;
    flag_days = false;
}


gnss_sdr_sample_counter_sptr gnss_sdr_make_sample_counter(double _fs, int32_t _interval_ms, size_t _size)
{
    gnss_sdr_sample_counter_sptr sample_counter_(new gnss_sdr_sample_counter(_fs, _interval_ms, _size));
    return sample_counter_;
}


int gnss_sdr_sample_counter::work(int noutput_items __attribute__((unused)),
    gr_vector_const_void_star &input_items __attribute__((unused)),
    gr_vector_void_star &output_items)
{
    auto *out = reinterpret_cast<Gnss_Synchro *>(output_items[0]);
    out[0] = Gnss_Synchro();
    out[0].Flag_valid_symbol_output = false;
    out[0].Flag_valid_word = false;
    out[0].Channel_ID = -1;
    out[0].fs = fs;
    if ((current_T_rx_ms % report_interval_ms) == 0)
        {
            current_s++;
            if ((current_s % 60) == 0)
                {
                    current_s = 0;
                    current_m++;
                    flag_m = true;
                    if ((current_m % 60) == 0)
                        {
                            current_m = 0;
                            current_h++;
                            flag_h = true;
                            if ((current_h % 24) == 0)
                                {
                                    current_h = 0;
                                    current_days++;
                                    flag_days = true;
                                }
                        }
                }

            if (flag_days)
                {
                    std::string day;
                    if (current_days == 1)
                        {
                            day = " day ";
                        }
                    else
                        {
                            day = " days ";
                        }
                    std::cout << "Current receiver time: " << current_days << day << current_h << " h " << current_m << " min " << current_s << " s\n";
                }
            else
                {
                    if (flag_h)
                        {
                            std::cout << "Current receiver time: " << current_h << " h " << current_m << " min " << current_s << " s\n";
                        }
                    else
                        {
                            if (flag_m)
                                {
                                    std::cout << "Current receiver time: " << current_m << " min " << current_s << " s\n";
                                }
                            else
                                {
                                    std::cout << "Current receiver time: " << current_s << " s\n";
                                }
                        }
                }
            if (flag_enable_send_msg)
                {
                    message_port_pub(pmt::mp("receiver_time"), pmt::from_double(static_cast<double>(current_T_rx_ms) / 1000.0));
                }
        }
    sample_counter += samples_per_output;
    out[0].Tracking_sample_counter = sample_counter;
    current_T_rx_ms += interval_ms;
    return 1;
}
