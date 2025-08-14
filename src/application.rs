/* application.rs
 *
 * Copyright ©2024 The GNOME Project
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 *
 */

use std::process::exit;

use gio::glib;
use gio::prelude::*;
use gio::subclass::prelude::*;

use crate::{http, settings::Settings};

const APPLICATION_ID: &str = "org.gnome.user-share.webdav";

mod imp {

	use std::cell::OnceCell;

	use gio::ApplicationHoldGuard;

	use super::*;

	#[derive(Default)]
	pub struct WebdavApplication {
		pub holder: OnceCell<ApplicationHoldGuard>,
	}

	#[gio::glib::object_subclass]
	impl ObjectSubclass for WebdavApplication {
		const NAME: &'static str = "WebdavApplication";
		type Type = super::WebdavApplication;
		type ParentType = gio::Application;
	}

	impl ObjectImpl for WebdavApplication {}

	impl ApplicationImpl for WebdavApplication {
		fn startup(&self) {
			self.parent_startup();

			let connection = self
				.obj()
				.dbus_connection()
				.expect("Unable to connect to d-bus.");
			let signal_handler = glib::clone!(
				#[weak(rename_to = app)]
				self,
				#[upgrade_or]
				glib::ControlFlow::Break,
				move || {
					app.obj().quit();

					glib::ControlFlow::Continue
				}
			);

			connection.connect_closed(|_, _, _| {
				http::down();

				exit(2)
			});

			glib::unix_signal_add_local(libc::SIGINT, signal_handler.clone());
			glib::unix_signal_add_local(libc::SIGHUP, signal_handler.clone());
			glib::unix_signal_add_local(libc::SIGTERM, signal_handler);

			Settings::connect_password_required_changed(|_, _| {
				http::down();
				http::up();
			});
		}

		fn activate(&self) {
			self.parent_activate();

			http::up();

			// Need to store a reference to the holder
			self.holder.get_or_init(|| self.obj().hold());
		}
	}
}

glib::wrapper! {
	pub struct WebdavApplication(ObjectSubclass<imp::WebdavApplication>)
		@extends gio::Application,
		@implements gio::ActionGroup, gio::ActionMap;
}

impl Default for WebdavApplication {
	fn default() -> Self {
		glib::Object::builder()
			.property("application-id", APPLICATION_ID)
			.property("flags", gio::ApplicationFlags::empty())
			.build()
	}
}
