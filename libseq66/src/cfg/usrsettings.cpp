/*
 *  This file is part of seq66.
 *
 *  seq66 is free software; you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software
 *  Foundation; either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  seq66 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with seq66; if not, write to the Free Software Foundation, Inc., 59 Temple
 *  Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file          usrsettings.cpp
 *
 *  This module declares/defines just some of the global (gasp!) variables
 *  in this application.
 *
 * \library       seq66 application
 * \author        Chris Ahlstrom
 * \date          2015-09-23
 * \updates       2021-06-26
 * \license       GNU GPLv2 or above
 *
 *  Note that this module also sets the remaining legacy global variables, so
 *  that they can be used by modules that have not yet been cleaned up.
 *
 *  Now, we finally sat down and did some measurements of the user interface,
 *  to try to figure out the relationships between the screen resolution and
 *  MIDI time resolution, so that we can understand some of the magic numbers
 *  in Seq24.
 *
 *  We start with one clue, a comment in perftime (IIRC) about units being 32
 *  ticks per pixels.  Note that "ticks" is equivalent to MIDI "pulses", and
 *  sometimes the word "division" is used for "pulses".  So let's solidy the
 *  nomenclature and notation here:
 *
\verbatim
    Symbol      Units           Value       Description

     qn         quarter note    -----       The default unit for a MIDI beat
     P0         pulses/qn       192         Seq24's PPQN value, a constant
     P          pulses/qn       -----       Any other selected PPQN value
     R          -----           -----       P / P0
     Wscreen    pixels          1920        Width of the screen, pixels
     Wperfqn    pixels          6           Song editor q-note width, constant
     Zperf      pulses/pixel    32          Song editor default zoom, constant
     Dperf      minor/major     4           Song editor beats shown per measure
     ?          pulses/pixel    -----       GUI-MIDI resolution from selected P
     S          -----           16          seqroll-to-perfroll width ratio
     Zseqmin    pulses/pixel    1           Seq editor max zoom in
     Zseq0      pulses/pixel    2           Seq editor default zoom
     Zseqmax    pulses/pixel    128 (32)    Seq editor max zoom out
\endverbatim
 *
 * Sequence Editor (seqroll):
 *
 *  Careful measuring on my laptop screen shows that the perfroll covers 80
 *  measures over 1920 pixels.
 *
\verbatim
    1920 pixels
    ----------- = 24 pixels/measure = 6 pixels/qn = Wperfqn
    80 measures
\endverbatim
 *
 * Song Editor (perfroll) Zoom:
 *
 *  The value of S = 16 reflects that the sequence editor piano roll,
 *  at its default zoom (2 pulses/pixel), has 16 times the width resolution
 *  of the performer/song editor piano roll (32 pulses/pixel).  This ratio
 *  (at the default zoom) will be preserved no matter what P (PPQN) is
 *  selected for the song.
 *
 *  The sequence editor supports zooms of 1 pulse/pixel, 2 pulses/pixel (it's
 *  default), and 4, 8, 16, and 32 pulses/pixel (the song editor's only zoom).
 *
 * Song Editor (perfedit, perfroll, pertime) Guides:
 *
 *                    pulses        major
 *  measureticks = P0 ------  Dperf -----
 *                     qn           minor
 *
 *     perfedit:  m_ppqn    m_standard_bpm
 *
 * Time Signature:
 *
 *  Changing the beats-per-measure of the seqroll to from the default 4 to 8
 *  makes the measure have 8 major divisions, each with the standard 16 minor
 *  divisions. An added note still covers only 4 minor divisions.
 *
 *  Changing the beat-width of the seqroll from the default 4 to 8 halves the
 *  pixel-width of reach measure.
 */

#include "seq66_features.hpp"           /* SEQ66_USE_ZOOM_POWER_OF_2        */
#include "cfg/settings.hpp"             /* seq66::rc(), seq66::usr()        */
#include "play/screenset.hpp"           /* seq66::screenset constants       */
#include "play/seq.hpp"                 /* seq66::seq::limit()              */
#include "util/strfunctions.hpp"        /* free functions in seq66 n'space  */

/*
 *  Do not document a namespace; it breaks Doxygen.
 */

namespace seq66
{

/**
 *  Provide limits for the option "--option scale=x.y".  Based on the minimum
 *  size of the main window specified in qsmainwnd.ui, 0.8 is the smallest one
 *  that can go well for both width and height.
 */

const double c_window_scale_min         = 0.5;  // 0.8f
const double c_window_scale_default     = 1.0f;
const double c_window_scale_max         = 3.0f;

/**
 *  These currently just exposed some values from the *.ui files.  The size of
 *  the main window.
 */

const int c_default_window_width        = 884;  // shrunken = 720, 0.82 664
const int c_default_window_height       = 602;  // shrunken = 480, 0.80 450
const int c_minimum_window_width        = 720;  // shrunken = 540, 0.75 540
const int c_minimum_window_height       = 480;  // shrunken = 360, 0.66 450

/**
 *  Minimum and maximum possible values for the global redraw rate.
 */

const int c_minimum_redraw              =  10;
const int c_maximum_redraw              = 100;

/**
 *  Provides the redraw time when recording, in ms.  Can Windows actually
 *  draw faster? :-D
 */

#if defined SEQ66_PLATFORM_WINDOWS
const int c_default_redraw_ms           = 25;
#else
const int c_default_redraw_ms           = 40;
#endif

/**
 *  These control sizes.  We'll try changing them and see what happens.
 *  Increasing these value spreads out the pattern grids a little bit and
 *  makes the Patterns panel slightly bigger.  Seems like it would be
 *  useful to make these values user-configurable.
 *
 *  Constants for the font class.  The c_text_x and c_text_y constants
 *  help define the "seqarea" size.  It looks like these two values are
 *  the character width (x) and height (y) in pixels.  Thus, these values
 *  would be dependent on the font chosen.  But that, currently, is
 *  hard-wired.
 */

const int c_text_x =  6;            /* does not include the inner padding   */
const int c_text_y = 12;            /* does include the inner padding       */

/**
 *  Constants for the main window, etc. The c_seqchars_x and c_seqchars_y
 *  constants help define the "seqarea" size.  These look like the number
 *  of characters per line and the number of lines of characters, in a
 *  pattern/sequence box.
 */

const int c_seqchars_x = 15;
const int c_seqchars_y =  5;

/**
 *  The c_seqarea_x and c_seqarea_y constants are derived from the width
 *  and heights of the default character set, and the number of characters
 *  in width, and the number of lines, in a pattern/sequence box.
 */

const int c_seqarea_x = c_text_x * c_seqchars_x;
const int c_seqarea_y = c_text_y * c_seqchars_y;

/**
 *  These control sizes.  We'll try changing them and see what happens.
 *  Increasing these value spreads out the pattern grids a little bit and
 *  makes the Patterns panel slightly bigger.  Seems like it would be
 *  useful to make these values user-configurable.
 */

const int c_mainwnd_spacing = 2;            // try 4 or 6 instead of 2

/**
 *  Provides the defaults for the progress box in the qloopbuttons.
 */

const double c_progress_box_width  = 0.80;
const double c_progress_box_height = 0.40;

/**
 *  Default constructor.
 */

usrsettings::usrsettings () :
    basesettings                (),
    m_midi_buses                (),     /* [user-midi-bus-definitions]      */
    m_instruments               (),     /* [user-instrument-definitions]    */

    /*
     * [user-interface-settings]
     */

    m_option_bits               (option_none),
    m_mainwnd_rows              (screenset::c_default_rows),
    m_mainwnd_cols              (screenset::c_default_columns),
    m_window_scale              (c_window_scale_default),
    m_window_scale_y            (c_window_scale_default),
    m_mainwnd_spacing           (0),
    m_current_zoom              (0),            // 0 is unsafe, but a feature
    m_global_seq_feature_save   (true),
    m_seqedit_scale             (c_scales_off),
    m_seqedit_key               (c_key_of_C),
    m_seqedit_bgsequence        (seq::limit()),
    m_progress_bar_thick        (true),
    m_inverse_colors            (false),
    m_window_redraw_rate_ms     (c_default_redraw_ms),

    /*
     * The members that follow are not yet part of the .usr file.
     */

    m_seqchars_x                (0),
    m_seqchars_y                (0),

    /*
     * [user-midi-settings]
     */

    m_default_ppqn              (SEQ66_DEFAULT_PPQN),
    m_midi_ppqn                 (SEQ66_DEFAULT_PPQN),
    m_use_file_ppqn             (false),
    m_file_ppqn                 (0),
    m_midi_beats_per_measure    (SEQ66_DEFAULT_BEATS_PER_MEASURE),
    m_midi_bpm_minimum          (0),
    m_midi_beats_per_minute     (SEQ66_DEFAULT_BPM),
    m_midi_bpm_maximum          (c_midibyte_value_max),
    m_midi_beat_width           (SEQ66_DEFAULT_BEAT_WIDTH),
    m_midi_buss_override        (null_buss()),
    m_velocity_override         (SEQ66_PRESERVE_VELOCITY),
    m_bpm_precision             (SEQ66_DEFAULT_BPM_PRECISION),
    m_bpm_step_increment        (SEQ66_DEFAULT_BPM_STEP_INCREMENT),
    m_bpm_page_increment        (SEQ66_DEFAULT_BPM_PAGE_INCREMENT),

    /*
     * Calculated from other member values in the normalize() function.
     */

    m_total_seqs                (0),
    m_seqs_in_set               (0),                /* set in normalize()   */
    m_gmute_tracks              (0),                /* same as max-tracks   */
    m_max_sequence              (seq::maximum()),
    m_mainwnd_x                 (c_default_window_width),   /* 780 */
    m_mainwnd_y                 (c_default_window_height),  /* 412 */

    /*
     * Constant values.
     */

    mc_min_zoom                 (SEQ66_MINIMUM_ZOOM),
    mc_max_zoom                 (SEQ66_MAXIMUM_ZOOM),
    mc_baseline_ppqn            (SEQ66_DEFAULT_PPQN),

    /*
     * Back to non-constant values.
     */

    m_save_user_config          (false),
    m_app_is_headless           (false),
    m_user_option_daemonize     (false),
    m_user_use_logfile          (false),
    m_user_option_logfile       (),

    /*
     * [user-ui-tweaks]
     */

    m_user_ui_key_height        (SEQ66_SEQKEY_HEIGHT_DEFAULT),
    m_user_ui_seqedit_in_tab    (true),
    m_user_ui_style_sheet       (""),
    m_resume_note_ons           (false),
    m_fingerprint_size          (32),
    m_progress_box_width        (c_progress_box_width),
    m_progress_box_height       (c_progress_box_height),
    m_session_manager           (session::none),
    m_session_url               (),
    m_in_session                (false),
    m_new_pattern_armed         (false),
    m_new_pattern_thru          (false),
    m_new_pattern_record        (false),
    m_new_pattern_qrecord       (false),
    m_new_pattern_recordstyle   (recordstyle::merge)
{
    // Empty body; it's no use to call normalize() here, see set_defaults().
}

/**
 *  Sets the default values.  For the m_midi_buses and
 *  m_instruments members, this function can only iterate over the
 *  current size of the vectors.  But the default size is zero!
 */

void
usrsettings::set_defaults ()
{
    m_midi_buses.clear();
    m_instruments.clear();
    m_option_bits = option_none;
    m_mainwnd_rows = screenset::c_default_rows;    // range: 4-8
    m_mainwnd_cols = screenset::c_default_columns; // range: 8-8
    m_window_scale = c_window_scale_default;    // range: 0.5 to 1.0 to 3.0
    m_window_scale_y = c_window_scale_default;
    m_mainwnd_spacing = c_mainwnd_spacing;      // range: 2-6, try 4 or 6
    m_current_zoom = SEQ66_DEFAULT_ZOOM;        // range: 1-128
    m_global_seq_feature_save = true;
    m_seqedit_scale = c_scales_off;             // scales::off to < scales::max
    m_seqedit_key = c_key_of_C;                 // range: 0-11
    m_seqedit_bgsequence = seq::limit();        // range -1, 0, 1, 2, ...
    m_progress_bar_thick = true;
    m_inverse_colors = false;
    m_window_redraw_rate_ms = c_default_redraw_ms;
    m_seqchars_x = 15;                      // range: 15-15
    m_seqchars_y =  5;                      // range: 5-5
    m_default_ppqn = SEQ66_DEFAULT_PPQN;    // range: 32 to 19200, default 192
    m_midi_ppqn = SEQ66_DEFAULT_PPQN;       // range: 32 to 19200, default 192
    m_use_file_ppqn = false;
    m_file_ppqn = 0;                        // range: 32 to 19200, default 0
    m_midi_beats_per_measure = SEQ66_DEFAULT_BEATS_PER_MEASURE; // range: 1-16
    m_midi_bpm_minimum = 0;                 // range: 0 to ???
    m_midi_beats_per_minute = SEQ66_DEFAULT_BPM;
    m_midi_bpm_maximum = c_midibyte_value_max;
    m_midi_beat_width = SEQ66_DEFAULT_BEAT_WIDTH;
    m_midi_buss_override = null_buss();             // 0xFF
    m_velocity_override = SEQ66_PRESERVE_VELOCITY;  // -1, 0 to 127
    m_bpm_precision = SEQ66_DEFAULT_BPM_PRECISION;
    m_bpm_step_increment = SEQ66_DEFAULT_BPM_STEP_INCREMENT;
    m_bpm_page_increment = SEQ66_DEFAULT_BPM_PAGE_INCREMENT;

    /*
     * Calculated from other member values in the normalize() function.
     *
     *  m_total_seqs
     *  m_seqs_in_set
     *  m_gmute_tracks
     *  m_max_sequence
     *
     * Constants:
     *
     *  mc_min_zoom
     *  mc_max_zoom
     *  mc_baseline_ppqn
     */

    m_mainwnd_x = c_default_window_width;
    m_mainwnd_y = c_default_window_height;

    m_save_user_config = false;
    m_app_is_headless = false;
    m_user_option_daemonize = false;
    m_user_use_logfile = false;
    m_user_option_logfile.clear();
    m_user_ui_key_height = SEQ66_SEQKEY_HEIGHT_DEFAULT;
    m_user_ui_seqedit_in_tab = true;
    m_user_ui_style_sheet = "";
    m_resume_note_ons = false;
    m_fingerprint_size = 32;
    m_progress_box_width = c_progress_box_width;
    m_progress_box_height = c_progress_box_height;
    m_session_manager = session::none;
    m_session_url.clear();
    m_in_session = false;
    m_new_pattern_armed = false;
    m_new_pattern_thru = false;
    m_new_pattern_record = false;
    m_new_pattern_qrecord = false;
    m_new_pattern_recordstyle = recordstyle::merge;
    normalize();                            // recalculate derived values
}

/**
 *  Calculate the derived values from the already-set values.
 *  Should we normalize the BPM increment values here, in case they
 *  are irregular?
 *
 *  gmute_tracks() is viable with variable set sizes only if we stick with the
 *  32 sets by 32 patterns, at this time. It's semantic meaning is......
 *
 *  m_max_sequence is now actually a constant (1024), so we enforce that here
 *  now.
 */

void
usrsettings::normalize ()
{
    m_seqs_in_set = m_mainwnd_rows * m_mainwnd_cols;
    m_gmute_tracks = m_seqs_in_set * m_seqs_in_set;
    m_total_seqs = m_seqs_in_set * c_max_sets;

    /*
     * Let's keep rows/columns separate from scaling, and keep shrunken()
     * merely to detect the need to hide some buttons.
     *
     * if (shrunken()) { (void) window_scale(0.80, 0.75); }
     */
}

std::string
usrsettings::session_manager_name () const
{
    if (wants_nsm_session())
        return std::string("nsm");
    else if (wants_lash_session())
        return std::string("lash");
    else
        return std::string("none");
}

/**
 *  Sets the desired session manager using a string value.
 *
 * \param sm
 *      Provides a string value of "nsm" for the Non/New Session Managers, or
 *      "lash" for LASH.  All other values set the m_session_manager code to
 *      session::none.
 */

void
usrsettings::session_manager (const std::string & sm)
{
    if (! test_option_bit(option_session_mgr))
    {
        session value = session::none;
        if (sm == "nsm")
            value = session::nsm;
        else if (sm == "lash")
            value = session::lash;

        m_session_manager = value;
        set_option_bit(option_session_mgr);
    }
}

int
usrsettings::scale_size (int value, bool shrinkmore) const
{
    float s = m_window_scale;
    if (shrinkmore)
        s *= 0.8;

    return int(s * value + 0.5);
}

int
usrsettings::scale_size_y (int value, bool shrinkmore) const
{
    float s = m_window_scale_y;
    if (shrinkmore)
        s *= 0.75;

    return int(s * value + 0.5);
}

int
usrsettings::mainwnd_x () const
{
    return m_window_scale != 1.0f ?
        int(scale_size(m_mainwnd_x)) : m_mainwnd_x ;
}

int
usrsettings::mainwnd_y () const
{
    return m_window_scale_y != 1.0f ?
        int(scale_size_y(m_mainwnd_y)) : m_mainwnd_y ;
}

int
usrsettings::mainwnd_x_min () const
{
    return int(scale_size(m_mainwnd_x, true));
}

int
usrsettings::mainwnd_y_min () const
{
    return int(scale_size_y(m_mainwnd_y, true));
}

/**
 *  Ultimately validated in the qloopbutton class.  Ignored if either is
 *  less than 0.0.
 */

void
usrsettings::progress_box_size (double w, double h)
{
    m_progress_box_width = (w >= 0.0) ? w : c_progress_box_width ;
    m_progress_box_height = (h >= 0.0) ? h : c_progress_box_height ;
}

/**
 *  Adds a user buss to the container, but only does so if the name
 *  parameter is not empty.
 */

bool
usrsettings::add_bus (const std::string & alias)
{
    bool result = ! alias.empty();
    if (result)
    {
        size_t currentsize = m_midi_buses.size();
        usermidibus temp(alias);
        result = temp.is_valid();
        if (result)
        {
            m_midi_buses.push_back(temp);
            result = m_midi_buses.size() == currentsize + 1;
        }
    }
    return result;
}

/**
 *  Adds a user instrument to the container, but only does so if the name
 *  parameter is not empty.
 */

bool
usrsettings::add_instrument (const std::string & name)
{
    bool result = ! name.empty();
    if (result)
    {
        size_t currentsize = m_instruments.size();
        userinstrument temp(name);
        result = temp.is_valid();
        if (result)
        {
            m_instruments.push_back(temp);
            result = m_instruments.size() == currentsize + 1;
        }
    }
    return result;
}

/**
 * \getter m_midi_buses[index] (internal function)
 *      If the index is out of range, then an invalid object is returned.
 *      This invalid object has an empty alias, and all the instrument
 *      numbers are -1.
 */

usermidibus &
usrsettings::private_bus (int index)
{
    static usermidibus s_invalid;                     /* invalid by default */
    if (index >= 0 && index < int(m_midi_buses.size()))
        return m_midi_buses[index];
    else
        return s_invalid;
}

/**
 * \getter m_midi_buses[index].instrument[channel]
 *      Currently this function is used, in the usrfile::parse()
 *      function.
 */

bool
usrsettings::set_bus_instrument (int index, int channel, int instrum)
{
    usermidibus & mb = private_bus(index);
    bool result = mb.is_valid();
    if (result)
        result = mb.set_instrument(channel, instrum);

    if (! result)
    {
        char temp[80];
        snprintf
        (
            temp, sizeof temp, "set_bus_instrument(%d, %d, %d) failed",
            index, channel, instrum
        );
        errprint(temp);
    }
    return result;
}

/**
 * \getter m_instruments[index]
 *      If the index is out of range, then a invalid object is returned.
 *      This invalid object has an empty(), instrument name, false for all
 *      controllers_active[] values, and empty controllers[] string values.
 */

userinstrument &
usrsettings::private_instrument (int index)
{
    static userinstrument s_invalid;
    if (index >= 0 && index < int(m_instruments.size()))
        return m_instruments[index];
    else
        return s_invalid;
}

/**
 * \setter m_midi_instrument_defs[index].controllers, controllers_active
 */

bool
usrsettings::set_instrument_controllers
(
    int index,
    int cc,
    const std::string & ccname,
    bool isactive
)
{
    userinstrument & mi = private_instrument(index);
    bool result = mi.is_valid();
    if (result)
        result = mi.set_controller(cc, ccname, isactive);

    if (! result)
    {
        char temp[80];
        snprintf
        (
            temp, sizeof temp, "set_instrument_controllers(%d, %d, %s) failed",
            index, cc, ccname.c_str()
        );
        errprint(temp);
    }
    return result;
}

/**
 * \setter m_window_scale and m_window_scale_y
 *
 *  For small device screens (800x480), use winscale = 0.85 and winscaley =
 *  0.55 approximately.
 */

bool
usrsettings::window_scale (float winscale, float winscaley)
{
    bool result =
    (
        winscale >= c_window_scale_min &&
        winscale <= c_window_scale_max
    );
    if (result && ! test_option_bit(option_scale))
    {
        m_window_scale = winscale;
        set_option_bit(option_scale);

        if (winscaley >= c_window_scale_min && winscaley <= c_window_scale_max)
            m_window_scale_y = winscaley;
        else
            m_window_scale_y = winscale;
    }
    return result;
}

bool
usrsettings::parse_window_scale (const std::string & source)
{
    bool result = false;
    std::vector<std::string> tokens = seq66::tokenize(source, "x");
    if (tokens.size() > 0)
    {
        double value1 = std::stod(tokens[0]);
        if (tokens.size() > 1)
        {
            double value2 = std::stod(tokens[1]);
            result = window_scale(value1, value2);
        }
        else
            result = window_scale(value1);
    }
    else
    {
        if (! source.empty())
        {
            double value = std::stod(source);
            result = window_scale(value);
        }
    }
    return result;
}

int
usrsettings::scale_font_size (int value) const
{
    int result = value;
    if (window_is_scaled())
    {
        result = m_window_scale <= m_window_scale_y ?
            scale_size(value) : scale_size_y(value) ;
    }
    return result;
}

/**
 * \setter m_mainwnd_rows
 *      This value is not modified unless the value parameter is
 *      between 4 and 8, inclusive.  The default value is 4.
 *      Dependent values are recalculated after the assignment.
 */

void
usrsettings::mainwnd_rows (int r)
{
    if ((r >= screenset::c_min_rows) && (r <= screenset::c_max_rows))
    {
        if (! test_option_bit(option_rows))
        {
            m_mainwnd_rows = r;
            normalize();
            set_option_bit(option_rows);
        }
    }
}

/**
 * \setter m_mainwnd_cols
 *      This value is not modified unless the value parameter is
 *      between 4 and 12, inclusive.  The default value is 8.
 *      Dependent values are recalculated after the assignment.
 */

void
usrsettings::mainwnd_cols (int c)
{
    if ((c >= screenset::c_min_columns) && (c <= screenset::c_max_columns))
    {
        if (! test_option_bit(option_columns))
        {
            m_mainwnd_cols = c;
            normalize();
            set_option_bit(option_columns);
        }
    }
}

/**
 * \setter m_seqchars_x
 *      This affects the size or crampiness of a pattern slot, and for now
 *      we will hardwire it to 15.
 */

void
usrsettings::seqchars_x (int value)
{
    if (value == 15)
    {
        m_seqchars_x = value;

        /*
         * Unnecessary: normalize();
         */
    }
}

/**
 * \setter m_seqchars_y
 *      This affects the size or crampiness of a pattern slot, and for now
 *      we will hardwire it to 5.
 */

void
usrsettings::seqchars_y (int value)
{
    if (value == 5)
    {
        m_seqchars_y = value;

        /*
         * Unnecessary: normalize();
         */
    }
}

/**
 * \setter m_mainwnd_spacing
 *      This value is not modified unless the value parameter is
 *      between 0 and 16, inclusive.  The default value is 2.
 *      Dependent values are recalculated after the assignment.
 */

void
usrsettings::mainwnd_spacing (int value)
{
    if (value >= 0 && value <= 16)
    {
        m_mainwnd_spacing = value;

        /*
         * Unnecessary: normalize();
         */
    }
}

/**
 * \setter m_current_zoom
 *      This value is not modified unless the value parameter is between 1 and
 *      512, inclusive.  The default value is 2.  Note that 0 is allowed as a
 *      special case, which allows the default zoom to be adjusted when the
 *      PPQN value is different from the default.
 */

void
usrsettings::zoom (int value)
{
    bool ok = value >= mc_min_zoom && value <= mc_max_zoom;
    if (ok || value == SEQ66_USE_ZOOM_POWER_OF_2)
        m_current_zoom = value;
}

void
usrsettings::default_ppqn (int value)
{
    if (value >= SEQ66_MINIMUM_PPQN && value <= SEQ66_MAXIMUM_PPQN)
        m_default_ppqn = value;
}

/**
 * \setter m_midi_ppqn
 *      This value can be set from 32 to 1920 (this upper limit will be
 *      determined by what Seq66 can actually handle).  The default value is
 *      192. However, if we're using file-ppqn as per the 'usr' file, then the
 *      given value will be used even if out-of-range.
 */

void
usrsettings::midi_ppqn (int value)
{
    if (! test_option_bit(option_ppqn))
    {
        if (value >= SEQ66_MINIMUM_PPQN && value <= SEQ66_MAXIMUM_PPQN)
        {
            m_midi_ppqn = value;
        }
        else
        {
            if (value == 0)
                m_use_file_ppqn = true;

            if (m_use_file_ppqn)
                m_midi_ppqn = value;
            else
                m_midi_ppqn = default_ppqn();
        }
        set_option_bit(option_ppqn);
    }
}

/**
 * \setter m_midi_beats_per_measure
 *      This value can be set from 1 to 20.  The default value is 4.
 */

void
usrsettings::midi_beats_per_bar (int value)
{
    if
    (
        value >= SEQ66_MINIMUM_BEATS_PER_MEASURE &&
        value <= SEQ66_MAXIMUM_BEATS_PER_MEASURE
    )
    {
        m_midi_beats_per_measure = value;
    }
}

/**
 * \setter m_midi_bpm_minimum
 *      This value can be set from 20 to 500.  The default value is 120.
 */

void
usrsettings::midi_bpm_minimum (midibpm value)
{
    if (value >= SEQ66_MINIMUM_BPM && value <= SEQ66_MAXIMUM_BPM)
        m_midi_bpm_minimum = value;
}

/**
 * \setter m_midi_beats_minute
 *      This value can be set from 20 to 500.  The default value is 120.
 */

void
usrsettings::midi_beats_per_minute (midibpm value)
{
    if (value >= SEQ66_MINIMUM_BPM && value <= SEQ66_MAXIMUM_BPM)
        m_midi_beats_per_minute = value;
}

/**
 * \setter m_midi_bpm_maximum
 *      This value can be set from 20 to 500.  The default value is 120.
 */

void
usrsettings::midi_bpm_maximum (midibpm value)
{
    if (value >= SEQ66_MINIMUM_BPM && value <= SEQ66_MAXIMUM_BPM)
        m_midi_bpm_maximum = value;
}

/**
 * \setter m_midi_beatwidth
 *      This value can be set to any power of 2 in the range from 1 to 16.
 *      The default value is 4.
 */

void
usrsettings::midi_beat_width (int bw)
{
    if (bw == 1 || bw == 2 || bw == 4 || bw == 8 ||bw == 16)
        m_midi_beat_width = bw;
}

/**
 *  This value can be set from 0 to c_busscount_max.  The default value is -1
 *  (0xFF), which means that there is no buss override, as defined by the
 *  inline function is_null_buss() in midibytes.hpp.  It provides a way to
 *  override the buss number for smallish MIDI files.  It replaces the
 *  buss-number read from the file.  This option is turned on by the --bus
 *  option, and is merely a convenience feature for the quick previewing of a
 *  tune.  (It's called "developer laziness".)
 */

void
usrsettings::midi_buss_override (bussbyte buss)
{
    if (is_valid_buss(buss))                /* good value or a null value   */
    {
        if (! test_option_bit(option_buss))
        {
            m_midi_buss_override = buss;
            set_option_bit(option_buss);
        }
    }
}

void
usrsettings::velocity_override (int vel)
{
    if (vel > SEQ66_MAX_NOTE_ON_VELOCITY)
        vel = SEQ66_MAX_NOTE_ON_VELOCITY;
    else if (vel <= 0)
        vel = SEQ66_PRESERVE_VELOCITY;

    m_velocity_override = vel;
}

/**
 * \setter m_bpm_precision
 */

void
usrsettings::bpm_precision (int precision)
{
    if (precision > SEQ66_MAXIMUM_BPM_PRECISION)
        precision = SEQ66_MAXIMUM_BPM_PRECISION;
    else if (precision < SEQ66_MINIMUM_BPM_PRECISION)
        precision = SEQ66_MINIMUM_BPM_PRECISION;

    m_bpm_precision = precision;
}

void
usrsettings::bpm_step_increment (midibpm increment)
{
    if (increment > SEQ66_MAXIMUM_BPM_INCREMENT)
        increment = SEQ66_MAXIMUM_BPM_INCREMENT;
    else if (increment < SEQ66_MINIMUM_BPM_INCREMENT)
        increment = SEQ66_MINIMUM_BPM_INCREMENT;

    m_bpm_step_increment = increment;
}

void
usrsettings::bpm_page_increment (midibpm increment)
{
    if (increment > SEQ66_MAXIMUM_BPM_INCREMENT)
        increment = SEQ66_MAXIMUM_BPM_INCREMENT;
    else if (increment < SEQ66_MINIMUM_BPM_INCREMENT)
        increment = SEQ66_MINIMUM_BPM_INCREMENT;

    m_bpm_page_increment = increment;
}

/**
 * \getter m_user_option_logfile
 *
 * \return
 *      This function returns rc().config_directory() + m_user_option_logfile
 *      if the latter does not contain a path marker ("/").  Otherwise, it
 *      returns m_user_option_logfile, which must be a full path specification
 *      to the desired log-file.
 */

std::string
usrsettings::option_logfile () const
{
    std::string result;
    if (! m_user_option_logfile.empty())
    {
        std::size_t slashpos = m_user_option_logfile.find_first_of("/");
        if (slashpos == std::string::npos)
        {
            result = rc().home_config_directory();
            char lastchar = result[result.length() - 1];
            if (lastchar != '/')
                result += '/';
        }
        result += m_user_option_logfile;
    }
    return result;
}

void
usrsettings::window_redraw_rate (int ms)
{
    if (ms >= c_minimum_redraw && ms <= c_maximum_redraw)
        m_window_redraw_rate_ms = ms;
}

bool
usrsettings::is_variset () const
{
    return
    (
        (m_mainwnd_rows != screenset::c_default_rows) ||
        (m_mainwnd_cols != screenset::c_default_columns)
    );
}

bool
usrsettings::is_default_mainwnd_size () const
{
    return
    (
        (m_mainwnd_rows == screenset::c_default_rows) &&
        (m_mainwnd_cols == screenset::c_default_columns)
    );
}

bool
usrsettings::vertically_compressed () const
{
    return m_mainwnd_rows < screenset::c_default_rows;
}

bool
usrsettings::horizontally_compressed () const
{
    return m_mainwnd_cols < screenset::c_default_columns;
}

/**
 *  The primary use of this function is to see if some buttons should be hidden in
 *  the main window, to allow a smaller size.
 */

bool
usrsettings::shrunken () const
{
    bool result =
        (mainwnd_rows() <= screenset::c_default_rows) &&
        (mainwnd_cols() < screenset::c_default_columns);

    if (! result)
        result = (m_window_scale < 0.80) || (m_window_scale_y < 0.75);

    return result;
}

/**
 *  Provides a debug dump of basic information to help debug a
 *  surprisingly intractable problem with all busses having the name and
 *  values of the last buss in the configuration.
 */

void
usrsettings::dump_summary ()
{
    int buscount = bus_count();
    printf("[user-midi-bus-definitions] %d busses\n", buscount);
    for (int b = 0; b < buscount; ++b)
    {
        const usermidibus & umb = bus(b);
        printf("   [user-midi-bus-%d] '%s'\n", b, umb.name().c_str());
    }

    int instcount = instrument_count();
    printf("[user-instrument-definitions] %d instruments\n", instcount);
    for (int i = 0; i < instcount; ++i)
    {
        const userinstrument & umi = instrument(i);
        printf("   [user-instrument-%d] '%s'\n", i, umi.name().c_str());
    }
    printf("\n");
    printf
    (
        "   mainwnd_rows() = %d\n"
        "   mainwnd_cols() = %d\n"
        "   seqs_in_set() = %d\n"
        "   gmute_tracks() = %d\n"
        "   max_sequence() = %d\n"
        ,
        mainwnd_rows(),
        mainwnd_cols(),
        seqs_in_set(),
        gmute_tracks(),
        max_sequence()
    );
    printf
    (
        "   seqchars_x(), _y() = %d, %d\n"
        "   mainwnd_spacing() = %d\n"
        ,
        seqchars_x(), seqchars_y(),
        mainwnd_spacing()
    );
    printf("\n");
    printf
    (
        "   midi_ppqn() = %d\n"
        "   midi_beats_per_bar() = %d\n"
        "   midi_beats_per_minute() = %g\n"
        "   midi_beat_width() = %d\n"
        "   midi_buss_override() = %d\n"
        ,
        midi_ppqn(),
        midi_beats_per_bar(),
        midi_beats_per_minute(),
        midi_beat_width(),
        int(midi_buss_override())
    );
}

}           // namespace seq66

/*
 * usrsettings.cpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

