/*!
 * \file glonass_l2_ca_telemetry_decoder_gs.cc
 * \brief Implementation of a GLONASS L2 C/A NAV data decoder block
 * \author Damian Miralles, 2018. dmiralles2009(at)gmail.com
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2020  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *          Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * -----------------------------------------------------------------------------
 */


#include "glonass_l2_ca_telemetry_decoder_gs.h"
#include "display.h"
#include "glonass_gnav_almanac.h"
#include "glonass_gnav_ephemeris.h"
#include "glonass_gnav_utc_model.h"
#include <glog/logging.h>
#include <gnuradio/io_signature.h>
#include <matio.h>          // for Mat_VarCreate
#include <pmt/pmt.h>        // for make_any
#include <pmt/pmt_sugar.h>  // for mp
#include <cmath>            // for floor, round
#include <cstddef>          // for size_t
#include <cstdlib>          // for abs
#include <exception>        // for exception
#include <iostream>         // for cout
#include <memory>           // for shared_ptr, make_shared
#include <vector>

#if HAS_STD_FILESYSTEM
#include <system_error>
namespace errorlib = std;
#if HAS_STD_FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#else
#include <boost/filesystem/operations.hpp>   // for remove
#include <boost/filesystem/path.hpp>         // for path, operator<<
#include <boost/filesystem/path_traits.hpp>  // for filesystem
#include <boost/system/error_code.hpp>       // for error_code
namespace fs = boost::filesystem;
namespace errorlib = boost::system;
#endif

#define CRC_ERROR_LIMIT 6


glonass_l2_ca_telemetry_decoder_gs_sptr
glonass_l2_ca_make_telemetry_decoder_gs(const Gnss_Satellite &satellite, const Tlm_Conf &conf)
{
    return glonass_l2_ca_telemetry_decoder_gs_sptr(new glonass_l2_ca_telemetry_decoder_gs(satellite, conf));
}


glonass_l2_ca_telemetry_decoder_gs::glonass_l2_ca_telemetry_decoder_gs(
    const Gnss_Satellite &satellite,
    const Tlm_Conf &conf) : gr::block("glonass_l2_ca_telemetry_decoder_gs", gr::io_signature::make(1, 1, sizeof(Gnss_Synchro)),
                                gr::io_signature::make(1, 1, sizeof(Gnss_Synchro)))
{
    // prevent telemetry symbols accumulation in output buffers
    this->set_max_noutput_items(1);
    // Ephemeris data port out
    this->message_port_register_out(pmt::mp("telemetry"));
    // Control messages to tracking block
    this->message_port_register_out(pmt::mp("telemetry_to_trk"));
    // initialize internal vars
    d_dump_filename = conf.dump_filename;
    d_dump = conf.dump;
    d_dump_mat = conf.dump_mat;
    d_satellite = Gnss_Satellite(satellite.get_system(), satellite.get_PRN());
    LOG(INFO) << "Initializing GLONASS L2 CA TELEMETRY DECODING";

    // preamble bits to sampled symbols
    int32_t n = 0;
    for (uint16_t d_preambles_bit : d_preambles_bits)
        {
            for (uint32_t j = 0; j < GLONASS_GNAV_TELEMETRY_SYMBOLS_PER_PREAMBLE_BIT; j++)
                {
                    if (d_preambles_bit == 1)
                        {
                            d_preambles_symbols[n] = 1;
                        }
                    else
                        {
                            d_preambles_symbols[n] = -1;
                        }
                    n++;
                }
        }

    d_symbol_history.set_capacity(GLONASS_GNAV_STRING_SYMBOLS);
    d_sample_counter = 0ULL;
    d_stat = 0;
    d_preamble_index = 0ULL;
    d_flag_frame_sync = false;
    d_flag_parity = false;
    d_TOW_at_current_symbol = 0;
    Flag_valid_word = false;
    delta_t = 0;
    d_CRC_error_counter = 0;
    d_flag_preamble = false;
    d_channel = 0;
    flag_TOW_set = false;
    d_preamble_time_samples = 0;
}


glonass_l2_ca_telemetry_decoder_gs::~glonass_l2_ca_telemetry_decoder_gs()
{
    DLOG(INFO) << "Glonass L2 Telemetry decoder block (channel " << d_channel << ") destructor called.";
    size_t pos = 0;
    if (d_dump_file.is_open() == true)
        {
            pos = d_dump_file.tellp();
            try
                {
                    d_dump_file.close();
                }
            catch (const std::exception &ex)
                {
                    LOG(WARNING) << "Exception in destructor closing the dump file " << ex.what();
                }
            if (pos == 0)
                {
                    errorlib::error_code ec;
                    if (!fs::remove(fs::path(d_dump_filename), ec))
                        {
                            LOG(WARNING) << "Error deleting temporary file";
                        }
                }
        }
    if (d_dump && (pos != 0) && d_dump_mat)
        {
            try
                {
                    save_matfile();
                }
            catch (const std::exception &ex)
                {
                    LOG(WARNING) << "Error saving the .mat file: " << ex.what();
                }
        }
}


int32_t glonass_l2_ca_telemetry_decoder_gs::save_matfile() const
{
    std::ifstream::pos_type size;
    const int32_t number_of_double_vars = 2;
    const int32_t number_of_int_vars = 2;
    const int32_t epoch_size_bytes = sizeof(uint64_t) + sizeof(double) * number_of_double_vars +
                                     sizeof(int32_t) * number_of_int_vars;
    std::ifstream dump_file;
    std::string dump_filename_ = d_dump_filename;

    std::cout << "Generating .mat file for " << dump_filename_ << '\n';
    dump_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
        {
            dump_file.open(dump_filename_.c_str(), std::ios::binary | std::ios::ate);
        }
    catch (const std::ifstream::failure &e)
        {
            std::cerr << "Problem opening dump file:" << e.what() << '\n';
            return 1;
        }
    // count number of epochs and rewind
    int64_t num_epoch = 0;
    if (dump_file.is_open())
        {
            size = dump_file.tellg();
            num_epoch = static_cast<int64_t>(size) / static_cast<int64_t>(epoch_size_bytes);
            if (num_epoch == 0LL)
                {
                    // empty file, exit
                    return 1;
                }
            dump_file.seekg(0, std::ios::beg);
        }
    else
        {
            return 1;
        }
    auto TOW_at_current_symbol_ms = std::vector<double>(num_epoch);
    auto tracking_sample_counter = std::vector<uint64_t>(num_epoch);
    auto TOW_at_Preamble_ms = std::vector<double>(num_epoch);
    auto nav_symbol = std::vector<int32_t>(num_epoch);
    auto prn = std::vector<int32_t>(num_epoch);

    try
        {
            if (dump_file.is_open())
                {
                    for (int64_t i = 0; i < num_epoch; i++)
                        {
                            dump_file.read(reinterpret_cast<char *>(&TOW_at_current_symbol_ms[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&tracking_sample_counter[i]), sizeof(uint64_t));
                            dump_file.read(reinterpret_cast<char *>(&TOW_at_Preamble_ms[i]), sizeof(double));
                            dump_file.read(reinterpret_cast<char *>(&nav_symbol[i]), sizeof(int32_t));
                            dump_file.read(reinterpret_cast<char *>(&prn[i]), sizeof(int32_t));
                        }
                }
            dump_file.close();
        }
    catch (const std::ifstream::failure &e)
        {
            std::cerr << "Problem reading dump file:" << e.what() << '\n';
            return 1;
        }

    // WRITE MAT FILE
    mat_t *matfp;
    matvar_t *matvar;
    std::string filename = dump_filename_;
    filename.erase(filename.length() - 4, 4);
    filename.append(".mat");
    matfp = Mat_CreateVer(filename.c_str(), nullptr, MAT_FT_MAT73);
    if (reinterpret_cast<int64_t *>(matfp) != nullptr)
        {
            std::array<size_t, 2> dims{1, static_cast<size_t>(num_epoch)};
            matvar = Mat_VarCreate("TOW_at_current_symbol_ms", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), TOW_at_current_symbol_ms.data(), 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            matvar = Mat_VarCreate("tracking_sample_counter", MAT_C_UINT64, MAT_T_UINT64, 2, dims.data(), tracking_sample_counter.data(), 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            matvar = Mat_VarCreate("TOW_at_Preamble_ms", MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims.data(), TOW_at_Preamble_ms.data(), 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            matvar = Mat_VarCreate("nav_symbol", MAT_C_INT32, MAT_T_INT32, 2, dims.data(), nav_symbol.data(), 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);

            matvar = Mat_VarCreate("PRN", MAT_C_INT32, MAT_T_INT32, 2, dims.data(), prn.data(), 0);
            Mat_VarWrite(matfp, matvar, MAT_COMPRESSION_ZLIB);  // or MAT_COMPRESSION_NONE
            Mat_VarFree(matvar);
        }
    Mat_Close(matfp);

    return 0;
}


void glonass_l2_ca_telemetry_decoder_gs::decode_string(const double *frame_symbols, int32_t frame_length)
{
    double chip_acc = 0.0;
    int32_t chip_acc_counter = 0;

    // 1. Transform from symbols to bits
    std::string bi_binary_code;
    bi_binary_code.reserve(frame_length / GLONASS_GNAV_TELEMETRY_SYMBOLS_PER_BIT);
    std::string relative_code;
    relative_code.reserve(GLONASS_GNAV_STRING_BITS);
    std::string data_bits;
    data_bits.reserve(GLONASS_GNAV_STRING_BITS + 1);

    // Group samples into bi-binary code
    for (int32_t i = 0; i < (frame_length); i++)
        {
            chip_acc += frame_symbols[i];
            chip_acc_counter += 1;

            if (chip_acc_counter == (GLONASS_GNAV_TELEMETRY_SYMBOLS_PER_BIT))
                {
                    if (chip_acc > 0)
                        {
                            bi_binary_code.push_back('1');
                            chip_acc_counter = 0;
                            chip_acc = 0;
                        }
                    else
                        {
                            bi_binary_code.push_back('0');
                            chip_acc_counter = 0;
                            chip_acc = 0;
                        }
                }
        }
    // Convert from bi-binary code to relative code
    for (int32_t i = 0; i < (GLONASS_GNAV_STRING_BITS); i++)
        {
            if (bi_binary_code[2 * i] == '1' && bi_binary_code[2 * i + 1] == '0')
                {
                    relative_code.push_back('1');
                }
            else
                {
                    relative_code.push_back('0');
                }
        }
    // Convert from relative code to data bits
    data_bits.push_back('0');
    for (int32_t i = 1; i < (GLONASS_GNAV_STRING_BITS); i++)
        {
            data_bits.push_back(((relative_code[i - 1] - '0') ^ (relative_code[i] - '0')) + '0');
        }

    // 2. Call the GLONASS GNAV string decoder
    d_nav.string_decoder(data_bits);

    // 3. Check operation executed correctly
    if (d_nav.get_flag_CRC_test() == true)
        {
            LOG(INFO) << "GLONASS GNAV CRC correct in channel " << d_channel << " from satellite " << d_satellite;
        }
    else
        {
            LOG(INFO) << "GLONASS GNAV CRC error in channel " << d_channel << " from satellite " << d_satellite;
        }
    // 4. Push the new navigation data to the queues
    if (d_nav.have_new_ephemeris() == true)
        {
            // get object for this SV (mandatory)
            d_nav.set_rf_link(d_satellite.get_rf_link());
            const std::shared_ptr<Glonass_Gnav_Ephemeris> tmp_obj = std::make_shared<Glonass_Gnav_Ephemeris>(d_nav.get_ephemeris());
            this->message_port_pub(pmt::mp("telemetry"), pmt::make_any(tmp_obj));
            LOG(INFO) << "GLONASS GNAV Ephemeris have been received in channel" << d_channel << " from satellite " << d_satellite;
            std::cout << TEXT_CYAN << "New GLONASS L2 GNAV message received in channel " << d_channel << ": ephemeris from satellite " << d_satellite << TEXT_RESET << '\n';
        }
    if (d_nav.have_new_utc_model() == true)
        {
            // get object for this SV (mandatory)
            const std::shared_ptr<Glonass_Gnav_Utc_Model> tmp_obj = std::make_shared<Glonass_Gnav_Utc_Model>(d_nav.get_utc_model());
            this->message_port_pub(pmt::mp("telemetry"), pmt::make_any(tmp_obj));
            LOG(INFO) << "GLONASS GNAV UTC Model data have been received in channel" << d_channel << " from satellite " << d_satellite;
            std::cout << TEXT_CYAN << "New GLONASS L2 GNAV message received in channel " << d_channel << ": UTC model parameters from satellite " << d_satellite << TEXT_RESET << '\n';
        }
    if (d_nav.have_new_almanac() == true)
        {
            const uint32_t slot_nbr = d_nav.get_alm_satellite_slot_number();
            const std::shared_ptr<Glonass_Gnav_Almanac> tmp_obj = std::make_shared<Glonass_Gnav_Almanac>(d_nav.get_almanac(slot_nbr));
            this->message_port_pub(pmt::mp("telemetry"), pmt::make_any(tmp_obj));
            LOG(INFO) << "GLONASS GNAV Almanac data have been received in channel" << d_channel << " in slot number " << slot_nbr;
            std::cout << TEXT_CYAN << "New GLONASS L2 GNAV almanac received in channel " << d_channel << " from satellite " << d_satellite << TEXT_RESET << '\n';
        }
    // 5. Update satellite information on system
    if (d_nav.get_flag_update_slot_number() == true)
        {
            LOG(INFO) << "GLONASS GNAV Slot Number Identified in channel " << d_channel;
            d_satellite.update_PRN(d_nav.get_ephemeris().d_n);
            d_satellite.what_block(d_satellite.get_system(), d_nav.get_ephemeris().d_n);
            d_nav.set_flag_update_slot_number(false);
        }
}


void glonass_l2_ca_telemetry_decoder_gs::set_satellite(const Gnss_Satellite &satellite)
{
    d_satellite = Gnss_Satellite(satellite.get_system(), satellite.get_PRN());
    DLOG(INFO) << "Setting decoder Finite State Machine to satellite " << d_satellite;
    DLOG(INFO) << "Navigation Satellite set to " << d_satellite;
}


void glonass_l2_ca_telemetry_decoder_gs::set_channel(int32_t channel)
{
    d_channel = channel;
    LOG(INFO) << "Navigation channel set to " << channel;
    // ############# ENABLE DATA FILE LOG #################
    if (d_dump == true)
        {
            if (d_dump_file.is_open() == false)
                {
                    try
                        {
                            d_dump_filename.append(std::to_string(d_channel));
                            d_dump_filename.append(".dat");
                            d_dump_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
                            d_dump_file.open(d_dump_filename.c_str(), std::ios::out | std::ios::binary);
                            LOG(INFO) << "Telemetry decoder dump enabled on channel " << d_channel << " Log file: " << d_dump_filename.c_str();
                        }
                    catch (const std::ifstream::failure &e)
                        {
                            LOG(WARNING) << "channel " << d_channel << ": exception opening Glonass TLM dump file. " << e.what();
                        }
                }
        }
}


int glonass_l2_ca_telemetry_decoder_gs::general_work(int noutput_items __attribute__((unused)), gr_vector_int &ninput_items __attribute__((unused)),
    gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
    int32_t corr_value = 0;
    int32_t preamble_diff = 0;

    auto **out = reinterpret_cast<Gnss_Synchro **>(&output_items[0]);            // Get the output buffer pointer
    const auto **in = reinterpret_cast<const Gnss_Synchro **>(&input_items[0]);  // Get the input buffer pointer

    Gnss_Synchro current_symbol{};  // structure to save the synchronization information and send the output object to the next block
    // 1. Copy the current tracking output
    current_symbol = in[0][0];
    d_symbol_history.push_back(current_symbol);  // add new symbol to the symbol queue
    d_sample_counter++;                          // count for the processed samples
    consume_each(1);

    d_flag_preamble = false;

    if (static_cast<int32_t>(d_symbol_history.size()) >= d_symbols_per_preamble)
        {
            // ******* preamble correlation ********
            for (int32_t i = 0; i < d_symbols_per_preamble; i++)
                {
                    if (d_symbol_history[i].Prompt_I < 0.0)  // symbols clipping
                        {
                            corr_value -= d_preambles_symbols[i];
                        }
                    else
                        {
                            corr_value += d_preambles_symbols[i];
                        }
                }
        }

    // ******* frame sync ******************
    if (d_stat == 0)  // no preamble information
        {
            if (abs(corr_value) >= d_symbols_per_preamble)
                {
                    // Record the preamble sample stamp
                    d_preamble_index = d_sample_counter;
                    LOG(INFO) << "Preamble detection for GLONASS L2 C/A SAT " << this->d_satellite;
                    // Enter into frame pre-detection status
                    d_stat = 1;
                    d_preamble_time_samples = d_symbol_history[0].Tracking_sample_counter;  // record the preamble sample stamp
                }
        }
    else if (d_stat == 1)  // possible preamble lock
        {
            if (abs(corr_value) >= d_symbols_per_preamble)
                {
                    // check preamble separation
                    preamble_diff = static_cast<int32_t>(d_sample_counter - d_preamble_index);
                    // Record the PRN start sample index associated to the preamble
                    d_preamble_time_samples = static_cast<double>(d_symbol_history[0].Tracking_sample_counter);
                    if (abs(preamble_diff - GLONASS_GNAV_PREAMBLE_PERIOD_SYMBOLS) == 0)
                        {
                            // try to decode frame
                            LOG(INFO) << "Starting string decoder for GLONASS L2 C/A SAT " << this->d_satellite;
                            d_preamble_index = d_sample_counter;  // record the preamble sample stamp
                            d_stat = 2;
                        }
                    else
                        {
                            if (preamble_diff > GLONASS_GNAV_PREAMBLE_PERIOD_SYMBOLS)
                                {
                                    d_stat = 0;  // start again
                                }
                            DLOG(INFO) << "Failed string decoder for GLONASS L2 C/A SAT " << this->d_satellite;
                        }
                }
        }
    else if (d_stat == 2)
        {
            // FIXME: The preamble index marks the first symbol of the string count. Here I just wait for another full string to be received before processing
            if (d_sample_counter == d_preamble_index + static_cast<uint64_t>(GLONASS_GNAV_STRING_SYMBOLS))
                {
                    // NEW GLONASS string received
                    // 0. fetch the symbols into an array
                    const int32_t string_length = GLONASS_GNAV_STRING_SYMBOLS - d_symbols_per_preamble;
                    std::array<double, GLONASS_GNAV_DATA_SYMBOLS> string_symbols{};

                    // ******* SYMBOL TO BIT *******
                    for (int32_t i = 0; i < string_length; i++)
                        {
                            if (corr_value > 0)
                                {
                                    string_symbols[i] = d_symbol_history[i + d_symbols_per_preamble].Prompt_I;  // because last symbol of the preamble is just received now!
                                }
                            else
                                {
                                    string_symbols[i] = -d_symbol_history[i + d_symbols_per_preamble].Prompt_I;  // because last symbol of the preamble is just received now!
                                }
                        }

                    // call the decoder
                    decode_string(string_symbols.data(), string_length);
                    if (d_nav.get_flag_CRC_test() == true)
                        {
                            d_CRC_error_counter = 0;
                            d_flag_preamble = true;               // valid preamble indicator (initialized to false every work())
                            d_preamble_index = d_sample_counter;  // record the preamble sample stamp (t_P)
                            if (!d_flag_frame_sync)
                                {
                                    d_flag_frame_sync = true;
                                    DLOG(INFO) << " Frame sync SAT " << this->d_satellite << " with preamble start at "
                                               << d_symbol_history[0].Tracking_sample_counter << " [samples]";
                                }
                        }
                    else
                        {
                            d_CRC_error_counter++;
                            d_preamble_index = d_sample_counter;  // record the preamble sample stamp
                            if (d_CRC_error_counter > CRC_ERROR_LIMIT)
                                {
                                    LOG(INFO) << "Lost of frame sync SAT " << this->d_satellite;
                                    d_flag_frame_sync = false;
                                    d_stat = 0;
                                }
                        }
                }
        }

    // UPDATE GNSS SYNCHRO DATA
    // 2. Add the telemetry decoder information
    if (this->d_flag_preamble == true and d_nav.get_flag_TOW_new() == true)
        // update TOW at the preamble instant
        {
            d_TOW_at_current_symbol = floor((d_nav.get_ephemeris().d_TOW - GLONASS_GNAV_PREAMBLE_DURATION_S) * 1000) / 1000;
            d_nav.set_flag_TOW_new(false);
        }
    else  // if there is not a new preamble, we define the TOW of the current symbol
        {
            d_TOW_at_current_symbol = d_TOW_at_current_symbol + GLONASS_L2_CA_CODE_PERIOD_S;
        }

    // if (d_flag_frame_sync == true and d_nav.flag_TOW_set==true and d_nav.get_flag_CRC_test() == true)

    // if(d_nav.flag_GGTO_1 == true  and  d_nav.flag_GGTO_2 == true and  d_nav.flag_GGTO_3 == true and  d_nav.flag_GGTO_4 == true) // all GGTO parameters arrived
    //     {
    //         delta_t = d_nav.A_0G_10 + d_nav.A_1G_10 * (d_TOW_at_current_symbol - d_nav.t_0G_10 + 604800.0 * (fmod((d_nav.WN_0 - d_nav.WN_0G_10), 64)));
    //     }

    if (d_flag_frame_sync == true and d_nav.is_flag_TOW_set() == true)
        {
            current_symbol.Flag_valid_word = true;
        }
    else
        {
            current_symbol.Flag_valid_word = false;
        }

    current_symbol.PRN = this->d_satellite.get_PRN();
    current_symbol.TOW_at_current_symbol_ms = round(d_TOW_at_current_symbol * 1000.0);
    // todo: glonass time to gps time should be done in observables block
    // current_symbol.TOW_at_current_symbol_ms -= static_cast<uint32_t>(delta_t) * 1000;

    if (d_dump == true)
        {
            // MULTIPLEXED FILE RECORDING - Record results to file
            try
                {
                    double tmp_double;
                    uint64_t tmp_ulong_int;
                    int32_t tmp_int;
                    tmp_double = d_TOW_at_current_symbol;
                    d_dump_file.write(reinterpret_cast<char *>(&tmp_double), sizeof(double));
                    tmp_ulong_int = current_symbol.Tracking_sample_counter;
                    d_dump_file.write(reinterpret_cast<char *>(&tmp_ulong_int), sizeof(uint64_t));
                    tmp_double = 0;
                    d_dump_file.write(reinterpret_cast<char *>(&tmp_double), sizeof(double));
                    tmp_int = (current_symbol.Prompt_I > 0.0 ? 1 : -1);
                    d_dump_file.write(reinterpret_cast<char *>(&tmp_int), sizeof(int32_t));
                    tmp_int = static_cast<int32_t>(current_symbol.PRN);
                    d_dump_file.write(reinterpret_cast<char *>(&tmp_int), sizeof(int32_t));
                }
            catch (const std::ifstream::failure &e)
                {
                    LOG(WARNING) << "Exception writing observables dump file " << e.what();
                }
        }

    // 3. Make the output (copy the object contents to the GNURadio reserved memory)
    *out[0] = current_symbol;

    return 1;
}
