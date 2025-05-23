/* === S Y N F I G ========================================================= */
/*!	\file widgets/widget_soundwave.cpp
**	\brief Widget for display a sound wave time-graph
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	......... ... 2019 Rodolfo Ribeiro Gomes
**
**	This file is part of Synfig.
**
**	Synfig is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 2 of the License, or
**	(at your option) any later version.
**
**	Synfig is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with Synfig.  If not, see <https://www.gnu.org/licenses/>.
**	\endlegal
*/

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <gui/widgets/widget_soundwave.h>

#include <cairomm/cairomm.h>
#include <gdkmm.h>
#include <glibmm/convert.h>

#include <gui/exception_guard.h>
#include <gui/helpers.h>
#include <gui/localization.h>
#include <gui/timeplotdata.h>

#ifndef WITHOUT_MLT
#include <Mlt.h>
#endif

#include <synfig/general.h>

#endif

using namespace studio;

const int default_frequency = 48000;
const int default_n_channels = 2;

Widget_SoundWave::MouseHandler::~MouseHandler() {}

Widget_SoundWave::Widget_SoundWave()
    : Widget_TimeGraphBase(),
	  frequency(default_frequency),
	  n_channels(default_n_channels),
	  n_samples(0),
	  channel_idx(0),
	  loading_error(false)
{
	add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::SCROLL_MASK | Gdk::POINTER_MOTION_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	setup_mouse_handler();

	set_default_page_size(255);
	set_zoom(1.0);

	int range_max = 255;
	int range_min = 0;
	int h = get_allocated_height();

	ConfigureAdjustment(range_adjustment)
		.set_lower(-range_max /*- 0.05*range_adjustment->get_page_size()*/)
		.set_upper(-range_min /*+ 0.05*range_adjustment->get_page_size()*/)
		.set_step_increment(range_adjustment->get_page_size()*20.0/h) // 20 pixels
		.set_value((range_max-range_min)/2)
		.finish();
}

Widget_SoundWave::~Widget_SoundWave()
{
	clear();
}

bool Widget_SoundWave::load(const synfig::filesystem::Path& filename)
{
	clear();

	std::lock_guard<std::mutex> lock(mutex);
	if (!do_load(filename)) {
		loading_error = true;
		queue_draw();
		return false;
	}
	loading_error = false;
	this->filename = filename;
	signal_file_loaded().emit(filename);
	queue_draw();
	return true;
}

void Widget_SoundWave::clear()
{
	std::lock_guard<std::mutex> lock(mutex);
	buffer.clear();
	this->filename.clear();
	loading_error = false;
	sound_delay = 0.0;
	channel_idx = 0;
	n_samples = 0;
	queue_draw();
}

void Widget_SoundWave::set_channel_idx(int new_channel_idx)
{
	if (channel_idx != new_channel_idx && new_channel_idx >= 0 && new_channel_idx < n_channels) {
		channel_idx = new_channel_idx;
		signal_specs_changed().emit();
		queue_draw();
	}
}

int Widget_SoundWave::get_channel_idx() const
{
	return channel_idx;
}

int Widget_SoundWave::get_channel_number() const
{
	return n_channels;
}

void Widget_SoundWave::set_delay(synfig::Time delay)
{
	if (delay == sound_delay)
		return;

	std::lock_guard<std::mutex> lock(mutex);
	sound_delay = delay;
	signal_delay_changed().emit();
	queue_draw();
}

const synfig::Time& Widget_SoundWave::get_delay() const
{
	return sound_delay;
}

bool Widget_SoundWave::on_event(GdkEvent* event)
{
	SYNFIG_EXCEPTION_GUARD_BEGIN()
	if (mouse_handler.process_event(event))
		return true;
	return Widget_TimeGraphBase::on_event(event);
	SYNFIG_EXCEPTION_GUARD_END_BOOL(true)
}

bool Widget_SoundWave::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	if (Widget_TimeGraphBase::on_draw(cr))
		return true;

	if (loading_error) {
		Glib::RefPtr<Pango::Layout> layout(Pango::Layout::create(get_pango_context()));
		layout->set_text(_("Audio file not supported"));

		cr->save();
		cr->set_source_rgb(0.8, 0.3, 0.3);
		cr->move_to(5, get_height()/2);
		layout->show_in_cairo_context(cr);
		cr->restore();
	}

	if (filename.empty())
		return true;

	if (buffer.empty())
		return true;

	if (!frequency || !n_channels)
		return true;

	cr->save();

	std::lock_guard<std::mutex> lock(mutex);


	Gdk::RGBA color = get_style_context()->get_color();
	cr->set_source_rgb(color.get_red(), color.get_green(), color.get_blue());

	constexpr int bytes_per_sample = 1;
	const int stride = frequency * bytes_per_sample * n_channels;
	const unsigned long max_index = buffer.size() - bytes_per_sample;

	const int middle_y = 127; // std::pow(2, bytes_per_sample) - 1;
	cr->move_to(0, middle_y);

	for (double x = 0; x < get_width(); x+=.1) {
		synfig::Time t = time_plot_data->get_t_from_pixel_coord(x);
		int value = middle_y;
		if (t >= sound_delay) {
			synfig::Time dt = t - sound_delay;
			unsigned long index = int(dt * stride) + channel_idx;
			if (index >= max_index)
				break;
			std::copy(buffer.begin() + index, buffer.begin() + index + bytes_per_sample, &value);
		}
		int y = time_plot_data->get_pixel_y_coord(value);
		cr->line_to(x, y);
	}
	cr->set_line_width(0.3);
	cr->stroke();

	draw_current_time(cr);

	int h = get_height();

	ConfigureAdjustment(range_adjustment)
		.set_step_increment(range_adjustment->get_page_size()*20.0/h) // 20 pixels
		.finish();

	cr->restore();
	return true;
}

void Widget_SoundWave::set_time_model(const etl::handle<TimeModel>& x)
{
	if (x) {
		previous_lower_time = x->get_lower();
		previous_upper_time = x->get_upper();
	}
	Widget_TimeGraphBase::set_time_model(x);
}

void Widget_SoundWave::on_time_model_changed()
{
	if (filename.empty())
		return;
	if (previous_lower_time == time_plot_data->time_model->get_lower()
		&& previous_upper_time == time_plot_data->time_model->get_upper())
		return;

	previous_lower_time = time_plot_data->time_model->get_lower();
	previous_upper_time = time_plot_data->time_model->get_upper();

	std::lock_guard<std::mutex> lock(mutex);
	buffer.clear();
	n_samples = 0;
	do_load(filename);
	queue_draw();
}

void Widget_SoundWave::setup_mouse_handler()
{
	mouse_handler.set_pan_enabled(true);
	mouse_handler.set_zoom_enabled(true);
	mouse_handler.set_scroll_enabled(true);
	mouse_handler.set_canvas_interface(canvas_interface);
	mouse_handler.signal_redraw_needed().connect(sigc::mem_fun(*this, &Gtk::Widget::queue_draw));
	mouse_handler.signal_focus_requested().connect(sigc::mem_fun(*this, &Gtk::Widget::grab_focus));
	mouse_handler.signal_zoom_in_requested().connect(sigc::mem_fun(*this, &Widget_SoundWave::zoom_in));
	mouse_handler.signal_zoom_out_requested().connect(sigc::mem_fun(*this, &Widget_SoundWave::zoom_out));
	mouse_handler.signal_zoom_horizontal_in_requested().connect(sigc::mem_fun(*this, &Widget_SoundWave::zoom_horizontal_in));
	mouse_handler.signal_zoom_horizontal_out_requested().connect(sigc::mem_fun(*this, &Widget_SoundWave::zoom_horizontal_out));
	mouse_handler.signal_scroll_up_requested().connect(sigc::mem_fun(*this, &Widget_SoundWave::scroll_up));
	mouse_handler.signal_scroll_down_requested().connect(sigc::mem_fun(*this, &Widget_SoundWave::scroll_down));
	mouse_handler.signal_scroll_right_requested().connect(sigc::mem_fun(*this, &Widget_SoundWave::scroll_right));
	mouse_handler.signal_scroll_left_requested().connect(sigc::mem_fun(*this, &Widget_SoundWave::scroll_left));
	mouse_handler.signal_panning_requested().connect(sigc::mem_fun(*this, &Widget_SoundWave::pan));
}

bool Widget_SoundWave::do_load(const synfig::filesystem::Path& filename)
{
#ifndef WITHOUT_MLT	
	std::string real_filename = Glib::filename_from_utf8(filename.u8string());
	Mlt::Profile profile;
	Mlt::Producer *track = new Mlt::Producer(profile, (std::string("avformat:") + real_filename).c_str());

	// That value is INT_MAX, which is our special value that means unknown or possibly a live source.
	// https://github.com/mltframework/mlt/issues/1073#issuecomment-2730111264
	constexpr int mlt_error_length = INT_MAX;

	if (!track->get_producer() || track->get_length() <= 0 || track->get_length() == mlt_error_length) {
		delete track;
		track = new Mlt::Producer(profile, (std::string("vorbis:") + real_filename).c_str());
		if (!track->get_producer() || track->get_length() <= 0 || track->get_length() == mlt_error_length) {
			delete track;
			return false;
		}
	}

	const int length = track->get_length();
	double fps = track->get_fps();
	int start_frame = 0;
	int end_frame = length;
	start_frame = synfig::clamp(start_frame, 0, length);
	end_frame = synfig::clamp(end_frame, 0, length);
	track->seek(start_frame);
	// check if audio is seekable
	if (track->position() != start_frame) {
		// Not seekable!
		synfig::error("Audio file not seekable, but a delay (%s) was set: %s", sound_delay.get_string(time_plot_data->time_model->get_frame_rate()).c_str(), filename.c_str());
	}
	unsigned char *outbuffer = nullptr;
	int bytes_written = 0;

	frequency = 0;
	n_channels = 0;

	for (int i = start_frame; i < end_frame; ++i) {
		std::unique_ptr<Mlt::Frame> frame(track->get_frame(0));
		if (!frame)
			break;

		if (!frequency)
			frequency = std::stoi(frame->get("audio_frequency"));
		if (!n_channels)
			n_channels = std::stoi(frame->get("audio_channels"));

		mlt_audio_format format = mlt_audio_u8;
		int bytes_per_sample = 1;
		int _frequency = frequency? frequency : default_frequency;
		int _channels = n_channels ? n_channels : default_n_channels;
		int _n_samples = 0;
		void * _buffer = frame->get_audio(format, _frequency, _channels, _n_samples);
		if (_buffer == nullptr) {
			synfig::warning("couldn't get sound frame #%i", i);
			break;
		}
		if (buffer.empty()) {
			synfig::warning("sound frame #%i got empty buffer", i);
			int buffer_length = (end_frame - start_frame) * _channels * bytes_per_sample * std::round(_frequency/fps);
			buffer.resize(buffer_length);
		}
		int _n_bytes = _n_samples * _channels * bytes_per_sample;
		if (bytes_written + _n_bytes > buffer.size()) {
			if (buffer.size() <= bytes_written) {
				synfig::error(_("Internal error: Widget_SoundWave: trying to read more bytes than buffer size: %i x %zu"), _n_bytes + bytes_written, buffer.size());
				break;
			}
			_n_bytes = buffer.size() - bytes_written;
		}
		std::copy(static_cast<unsigned char*>(_buffer), static_cast<unsigned char*>(_buffer) + _n_bytes, buffer.begin() + bytes_written);
		bytes_written += _n_bytes;
		outbuffer += _n_bytes;
		frequency = _frequency;
		n_channels = _channels;
		n_samples += _n_samples;
	}
	if (channel_idx > n_channels)
		channel_idx = 0;
	queue_draw();
	delete track;
#endif
	return true;
}
