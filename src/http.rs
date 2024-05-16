/* http.rs
 *
 * Copyright ©2024 The GNOME Project
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 *
 */

use std::net::{Ipv4Addr, SocketAddrV4, UdpSocket};
use std::os::unix::fs::PermissionsExt;
use std::path::PathBuf;
use std::process::{Child, Command};
use std::sync::{LazyLock, Mutex};

use gettextrs::gettext;
use gio::glib;
// TODO: Doesn't compile
#[cfg(feature = "selinux")]
use selinux;

use crate::config::*;
use crate::settings;

const LOG_DOMAIN: &str = "gnome-user-share: http";

static HTTPD_PID: Mutex<Option<Child>> = Mutex::new(None);

static SOCKET: LazyLock<UdpSocket> = LazyLock::new(|| {
	UdpSocket::bind(SocketAddrV4::new(Ipv4Addr::new(0, 0, 0, 0), 0))
		.expect("Failed to open a socket")
});

static HTTPD_PORT: LazyLock<u16> = LazyLock::new(|| {
	SOCKET
		.local_addr()
		.expect("Failed to bind the socket to an address")
		.port()
});

static SHARE_NAME: LazyLock<String> = LazyLock::new(|| {
	let host_name = glib::host_name();
	let user_name = glib::user_name().into_string().unwrap();

	if host_name == "localhost" {
		// Translators: The {} will get filled in with the user name
		// of the user, to form a genitive. If this is difficult to
		// translate correctly so that it will work correctly in your
		// language, you may use something equivalent to
		// "Public files of {}", or leave out the {} altogether.
		// In the latter case, please put "{0}" somewhere in the string,
		// which will match the user name string passed by the C code,
		// but not put the user name in the final string. This is to
		// avoid the warning that msgfmt might otherwise generate.
		gettext!("{}'s public files", user_name)
	} else {
		// Translators: This is similar to the string before, only it
		// has the hostname in it too.
		// TODO: position correction
		gettext!("{}'s public files on {}", user_name, host_name)
	}
});

static PUBLIC_DIR: LazyLock<PathBuf> = LazyLock::new(|| {
	let path = glib::user_special_dir(glib::UserDirectory::PublicShare)
		.unwrap_or_else(|| glib::home_dir().join("Public"));

	if glib::mkdir_with_parents(path.clone(), 755) != 0 {
		glib::g_warning!(
			LOG_DOMAIN,
			"Creating a public folder in \"{path:#?}\" has failed!"
		);
	}

	path
});

static HTTPD_PROGRAM_PATH: LazyLock<PathBuf> = LazyLock::new(|| {
	[
		HTTPD_PROGRAM,
		"/usr/sbin/httpd",
		"/usr/sbin/httpd2",
		"/usr/sbin/apache2",
	]
	.iter()
	.find_map(|&path| {
		let file = PathBuf::from(path);
		if !path.is_empty()
			&& !file.is_dir()
			&& file.metadata().ok()?.permissions().mode() & 0o111 != 0
		{
			return Some(file);
		}
		None
	})
	.expect("Failed to find an httpd server executable file.")
});

static HTTPD_MODULES: LazyLock<PathBuf> = LazyLock::new(|| {
	[
		HTTPD_MODULES_PATH,
		"/etc/httpd/modules",
		"/usr/lib/apache2/modules",
	]
	.iter()
	.find_map(|&path| {
		let file = PathBuf::from(path);
		if !path.is_empty() && file.is_dir() {
			if let Ok(d) = file.metadata() {
				if d.permissions().mode() & 0o111 != 0 {
					return Some(file);
				}
			}
		}
		None
	})
	.expect("Failed to find an httpd modules directory.")
});

static HTTPD_CONFIG_FILE: LazyLock<PathBuf> = LazyLock::new(|| {
	let cmd_output = Command::new(HTTPD_PROGRAM_PATH.clone())
		.arg("-v")
		.output()
		.expect("Could not check the httpd server version.");
	let output_string = String::from_utf8(cmd_output.stdout)
		.expect("Could not convert command output to a UTF-8 string.");

	let found = output_string
		.split_once('.')
		.and_then(|(a, b)| {
			let major = a
				.to_owned()
				.chars()
				.rev()
				.map_while(|c| c.is_ascii_digit().then_some(c))
				.collect::<Vec<char>>()
				.iter()
				.rev()
				.collect::<String>();
			let minor = b
				.to_owned()
				.chars()
				.map_while(|c| c.is_ascii_digit().then_some(c))
				.collect::<String>();

			(!major.is_empty() && !minor.is_empty()).then(|| format!("{major}.{minor}"))
		})
		// Fall back to version 2.4
		.unwrap_or("2.4".to_owned());

	PathBuf::from(format!("{HTTPD_CONFIG_TEMPLATE_PREFIX}{found}.conf"))
});

pub fn up() {
	let mut guard = HTTPD_PID.lock().expect("Poisoned mutex, terminating...");

	if guard.as_mut().is_none() {
		let port = HTTPD_PORT.clone();
		let password_required = settings::Settings::password_required();
		let home = glib::home_dir();
		// Translators: Don't translate the name in quotes!
		let login_label = &gettext("Please log in as the user “guest”");
		let mut command = Command::new(HTTPD_PROGRAM_PATH.clone().into_os_string());

		// Add arguments
		command.args(["-f", HTTPD_CONFIG_FILE.to_str().unwrap()]);
		command.args(["-C", &format!("Listen {port}")]);

		match password_required {
			settings::PasswordRequired::Always => {
				command.args(["-D", "RequirePasswordAlways"]);
			}
			settings::PasswordRequired::OnWrite => {
				command.args(["-D", "RequirePasswordOnWrite"]);
			}
			settings::PasswordRequired::Never => {}
		};

		// Add environment variables
		for &(var, val) in &[
			("HOME", home.to_str().unwrap()),
			("XDG_PUBLICSHARE_DIR", PUBLIC_DIR.to_str().unwrap()),
			("XDG_CONFIG_HOME", glib::user_config_dir().to_str().unwrap()),
			("GUS_SHARE_NAME", &SHARE_NAME),
			("GUS_LOGIN_LABEL", login_label),
			("HTTP_MODULES_PATH", HTTPD_MODULES.to_str().unwrap()),
			("LANG", "C"),
		] {
			command.env(var, val);
		}

		#[cfg(feature = "selinux")]
		unsafe {
			std::os::unix::process::CommandExt::pre_exec(&mut command, || {
				let x = selinux::SecurityContext::current(false);

				selinux::SecurityContext::set_for_next_exec(x)
			});
		}

		match command.current_dir(home).spawn() {
			Ok(child) => {
				guard.replace(child);
			}
			Err(e) => {
				glib::g_warning!(LOG_DOMAIN, "Error spawning httpd: {}", e.to_string());
			}
		}
	};
}

pub fn down() {
	HTTPD_PID
		.lock()
		.expect("Poisoned mutex, terminating...")
		.take()
		.as_mut()
		.map(|process| {
			process.kill().expect("Failed to kill httpd process");
		});
}
