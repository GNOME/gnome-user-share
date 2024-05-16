/* settings.rs
 *
 * Copyright ©2024 The GNOME Project
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 *
 */

use std::ops::Deref;
use std::str::FromStr;
use std::sync::LazyLock;

use gio::glib;
use gio::prelude::*;
use glib::thread_guard::ThreadGuard;

static LOG_DOMAIN: &str = "gnome-user-share: settings";

pub struct Settings(ThreadGuard<gio::Settings>);

static SETTINGS: LazyLock<Settings> = LazyLock::new(Settings::default);

pub enum PasswordRequired {
	Never,
	OnWrite,
	Always,
}

impl FromStr for PasswordRequired {
	type Err = &'static str;

	fn from_str(value: &str) -> Result<Self, Self::Err> {
		match value {
			"never" => Ok(Self::Never),
			"on_write" => Ok(Self::OnWrite),
			"always" => Ok(Self::Always),
			_ => Err("Option doesn't exist"),
		}
	}
}

impl Settings {
	const GNOME_USER_SHARE_SCHEMAS: &str = "org.gnome.desktop.file-sharing";

	const FILE_SHARING_REQUIRE_PASSWORD: &'static str = "require-password";

	pub fn password_required() -> PasswordRequired {
		let value = SETTINGS.string(Self::FILE_SHARING_REQUIRE_PASSWORD);

		str::parse(&value).unwrap_or_else(|_| {
			let key = Self::FILE_SHARING_REQUIRE_PASSWORD;

			glib::g_warning!(LOG_DOMAIN, "Option {value} is invalid for property {key}");

			PasswordRequired::Never
		})
	}

	pub fn connect_password_required_changed<F>(callback: F) -> glib::SignalHandlerId
	where
		F: Fn(&gio::Settings, &str) + 'static,
	{
		SETTINGS.connect_changed(Some(Self::FILE_SHARING_REQUIRE_PASSWORD), callback)
	}
}

impl Default for Settings {
	fn default() -> Self {
		Self(ThreadGuard::new(gio::Settings::new(
			Self::GNOME_USER_SHARE_SCHEMAS,
		)))
	}
}

impl Deref for Settings {
	type Target = gio::Settings;

	fn deref(&self) -> &Self::Target {
		self.0.get_ref()
	}
}
