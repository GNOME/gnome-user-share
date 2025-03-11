/* utilities.rs
 *
 * Copyright ©2025 The GNOME Project
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 *
 */

#[macro_use]
pub(crate) mod utilities {
	macro_rules! warning_exit {
		($str:expr) => {
			glib::g_warning!(LOG_DOMAIN, $str);

			std::process::exit(libc::EXIT_FAILURE)
		};
	}
}

pub(crate) use warning_exit;
