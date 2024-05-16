/* main.rs
 *
 * Copyright ©2024 The GNOME Project
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 *
 */

mod application;
mod config;
mod http;
mod settings;

use gettextrs::{bind_textdomain_codeset, bindtextdomain, textdomain};
use gio::glib;
use gio::prelude::*;

use application::WebdavApplication;
use config::{GETTEXT_PACKAGE, LOCALEDIR};

static LOG_DOMAIN: &str = "gnome-user-share: main";

const APPLICATION_ID: &str = "org.gnome.user-share.webdav";

use libc::uid_t;

fn main() -> glib::ExitCode {
	let user_id: uid_t;

	unsafe {
		user_id = libc::getuid();
	}

	if user_id == 0 {
		glib::g_warning!(
			LOG_DOMAIN,
			"gnome-user-share cannot be started as root for security reasons."
		);

		return glib::ExitCode::FAILURE;
	}

	// Set up gettext translations
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR).expect("Unable to bind the text domain");
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8")
		.expect("Unable to set the text domain encoding");
	textdomain(GETTEXT_PACKAGE).expect("Unable to switch to the text domain");

	let app = WebdavApplication::new(APPLICATION_ID, gio::ApplicationFlags::empty());

	let result = app.run();

	http::down();

	result
}
