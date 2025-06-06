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
use std::process::Command;
use std::sync::{LazyLock, Mutex};
use std::thread::sleep;
use std::time::Duration;

use gettextrs::gettext;
use gio::glib;
#[cfg(feature = "selinux")]
use selinux;

use crate::config::*;
use crate::settings;
use crate::utilities::warning_exit;

const LOG_DOMAIN: &str = "gnome-user-share: http";

static HTTPD_PID: Mutex<Option<libc::pid_t>> = Mutex::new(None);

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
		gettext("{}'s public files").replace("{}", &user_name)
	} else {
		// Translators: This is similar to the string before, only it
		// has the hostname in it too.
		gettext("{0}'s public files on {1}")
			.replace("{0}", &user_name)
			.replace("{1}", &host_name)
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

static USER_CONFIG_DIR: LazyLock<PathBuf> = LazyLock::new(|| {
	let path = glib::user_config_dir();

	if glib::mkdir_with_parents(path.join("user-share"), 755) != 0 {
		glib::g_warning!(
			LOG_DOMAIN,
			"Creating a config folder in \"{path:#?}\" has failed!"
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
		(!path.is_empty()
			&& !file.is_dir()
			&& file.metadata().ok()?.permissions().mode() & 0o111 != 0)
			.then_some(file)
	})
	.unwrap_or_else(|| {
		warning_exit!("Failed to find an httpd server executable file.");
	})
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
		(!path.is_empty()
			&& file.is_dir()
			&& file.metadata().ok()?.permissions().mode() & 0o111 != 0)
			.then_some(file)
	})
	.unwrap_or_else(|| {
		warning_exit!("Failed to find an httpd modules directory.");
	})
});

static HTTPD_CONFIG_FILE: LazyLock<PathBuf> = LazyLock::new(|| {
	let cmd_output = Command::new(HTTPD_PROGRAM_PATH.as_os_str())
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

fn get_httpd_pid() -> Option<libc::pid_t> {
	// This file generated by httpd contains its PID
	let path = USER_CONFIG_DIR.join("user-share").join("pid");

	// Retry a few times and wait in between
	Vec::from_iter(0..5).iter().find_map(|_| {
		std::fs::read_to_string(&path)
			.ok()
			.and_then(|pidfile| pidfile.trim().parse::<libc::pid_t>().ok())
			.or_else(|| {
				sleep(Duration::from_secs(1));
				None
			})
	})
}

fn get_own_httpd_pid() -> Option<libc::pid_t> {
	let guard = HTTPD_PID.lock().expect("Poisoned mutex, terminating...");

	get_httpd_pid().and_then(|file_pid| match *guard {
		Some(http_pid) if http_pid == file_pid => Some(http_pid),
		_ => None,
	})
}

fn get_httpd_command() -> Command {
	let home = glib::home_dir();
	let port = *HTTPD_PORT;
	let password_required = settings::Settings::password_required();
	// Translators: Don't translate the name in quotes!
	let login_label = &gettext("Please log in as the user “guest”");
	let mut command = Command::new(HTTPD_PROGRAM_PATH.as_os_str());

	command.current_dir(&home);

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
		("XDG_CONFIG_HOME", USER_CONFIG_DIR.to_str().unwrap()),
		("GUS_SHARE_NAME", &SHARE_NAME),
		("GUS_LOGIN_LABEL", login_label),
		("HTTP_MODULES_PATH", HTTPD_MODULES.to_str().unwrap()),
		("LANG", "C"),
	] {
		command.env(var, val);
	}

	#[cfg(feature = "selinux")]
	unsafe {
		// If selinux is enabled, avoid transitioning to the httpd_t context,
		// as this normally means you can't read the user's home directory.
		if !matches!(selinux::current_mode(), selinux::SELinuxMode::NotRunning) {
			std::os::unix::process::CommandExt::pre_exec(&mut command, || {
				let x = selinux::SecurityContext::current(false)
					.expect("Cannot obtain current SELinux security context.");

				selinux::SecurityContext::set_for_next_exec(&x)
					.expect("Cannot set SELinux security context.");
				Ok(())
			});
		}
	}

	command
}

pub fn up() {
	let mut guard = HTTPD_PID.lock().expect("Poisoned mutex, terminating...");

	// Delete the old pid file created by httpd before trying to spawn a new one.
	let _ = std::fs::remove_file(USER_CONFIG_DIR.join("user-share").join("pid"));

	match get_httpd_command().status() {
		Ok(status) => {
			if status.success() {
				*guard = get_httpd_pid();
			} else if let Some(code) = status.code() {
				glib::g_warning!(
					LOG_DOMAIN,
					"Error spawning httpd: exited with non-zero status: {}",
					code
				);
			} else {
				glib::g_warning!(
					LOG_DOMAIN,
					"Error spawning httpd: spawning process was terminated by a signal"
				);
			}
		}
		Err(e) => {
			glib::g_warning!(LOG_DOMAIN, "Error spawning httpd: {}", e.to_string());
		}
	}
}

pub fn down() {
	get_own_httpd_pid().inspect(|&pid| unsafe {
		libc::kill(pid, libc::SIGTERM);

		// Allow child time to die, we can't waitpid, because its not a direct child
		sleep(Duration::from_secs(1));
	});
}
