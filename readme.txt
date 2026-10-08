           \\|//
            O O
-------.oOO-( )-OOo.--------
      WHAT ON EARTH ARE
      YOU DOING IN HERE?
 ---------------------------


Right... If you are here, there must be a reason. So I had better hand over
whatever you might need. This file lives on the clock's card and was written
for whoever only has the clock at hand: no PC, no repository, just an SSH
terminal and courage.

User:   pi. The password is whatever was chosen when the card was built.
Log in: ssh pi@nixie.local   (or ssh pi@<clock-ip>)


===============================================================================
 HOW IT WORKS
===============================================================================

The clock runs on 3 applications:

 1 - camera. Watches the camera (duh) looking for faces and motion. When it
     spots somebody, it sends a signal to nixie.

 2 - nixie. Handles the signals it receives (and raises others internally)
     and drives all the hardware: tubes, bargraph, RGB LEDs, brightness. It
     also gets the time over NTP and the weather forecast from the internet.

 3 - Web site. lighttpd serves the configuration pages. The forms are
     handled by nixie.cgi, which writes nixie.json and tells nixie.

Besides the applications, there are boot scripts that find out whether the
clock joined the Wi-Fi or needs to open a hotspot.


===============================================================================
 IT'S ON. NOW WHAT?
===============================================================================

When the clock powers up, it tries to join the stored Wi-Fi network.
If it finds it, it connects and carries on.

If it doesn't, it:
  a) opens an open hotspot called "NixieClock";
  b) shows the IP to connect to on the display. Something like 192.168.4.1;
  c) stays like that forever, or until the stored network comes up. That
     happens when the power goes out and comes back: the clock usually boots
     faster than the router. When the network returns, it notices and
     restarts.

To reach the site in hotspot mode: put your phone in airplane mode, turn on
Wi-Fi only and join the "NixieClock" network. Open the browser and type the
IP shown on the tubes. Actually, any address works: google.com,
facebook.com, whatever, it all lands on the configuration site.

With the clock on the home network, the site is at http://nixie.local/ or
http://<clock-ip>/.


===============================================================================
 THE SITE
===============================================================================

Wi-Fi
  Network SSID and password. Mind upper and lower case, because this is
  Linux. The stored password is not shown on the page: type it again to save.
  Saving restarts the clock.

Location
  Latitude, longitude and the weather forecast key. Get the coordinates from
  Google Maps or any map service. Negative for south and west. The key is
  free at https://www.weatherapi.com/. Without it the clock works, but shows
  no temperature and uses fixed hours instead of sunrise and sunset. The
  stored key is not shown either: a blank field keeps the current one, and
  the "Delete" box removes it.

NTP servers
  Time zone (daylight saving comes with it) and up to four servers. They go
  straight to the system: the time zone through timedatectl, the servers to
  /etc/systemd/timesyncd.conf.d/nixie.conf. The stored ones SHOULD work, but
  you never know.

Detection
  Face, motion or both, how long the display stays on, the camera's frames
  per second and the motion threshold. For faces: the detector (improved
  LBP, LBP or Haar), the scale factor, minimum neighbors, how many frames in
  a row confirm a face and the sizes searched. Every field has a hint on what
  it changes. Takes effect immediately, nothing restarts. The tab also shows
  what the camera is really using: the frame it delivers, the detector and
  the face sizes the search tries. If the requested range holds none, the
  camera widens it on its own and the tab says so.

Tubes
  Overall brightness. The bargraph minimum and maximum set the position of
  the bar that shows the weekday and the current temperature. It needs
  readjusting now and then. While you change the values, the tube shows the
  bar: set the minimum so the bar sits on zero and the maximum so it reaches
  45°C. Daytime hours and cathode regeneration live here too: automatic, up
  to 6 sessions a day, each at the time you choose and with as many 0 to 9
  sweeps as you like, and manual, on a chosen tube. An automatic session
  only starts with the clock dark: if somebody is in front of it at that
  time, it waits for the next one.

Logs
  The clock's current log, no SSH needed.


===============================================================================
 SOMETHING BROKE
===============================================================================

On error, the clock shows a code on the display:

 "99   1"  camera configuration failure: the chosen face detector
           (~/nixiepi/*.xml) is missing or corrupt. Picking another one in
           the Detection tab also fixes it.
 "99   2"  camera hardware failure: bad contact, or it is dead.
 "99   3"  no Wi-Fi. Defined but never implemented. If this shows up, I have
           no idea how it happened.
 "99   x"  Anything else: just turn this shit off and forget about it. Bury it very,
           very deep.

Where to look:

  tail -f /tmp/nixie.txt          the clock's log (same as the Logs tab)
  cat /tmp/hotspot_debug.log      what the network script decided, at boot
                                  and on every retry from hotspot mode
  cat /tmp/network_mode           WIFI or HOTSPOT
  ls /tmp/wifi.txt                if it exists, the stored network is in range
  journalctl -u nixie -u camera -f

Everything in /tmp vanishes on every boot. On purpose: the card thanks you.


===============================================================================
 WHERE EVERYTHING IS
===============================================================================

  ~/nixiepi/nixie, camera         the two programs
  ~/nixiepi/*.xml                 OpenCV face detectors. Which one is used
                                  is picked in the site's Detection tab.
  ~/nixiepi/services.sh           stops and starts nixie and camera
  ~/nixiepi/update_bins.sh        installs new binaries (see below)

  ~/www/                          the site
  ~/www/json/nixie.json           ALL the configuration (~/nixie.json is a
                                  link to it). It holds the Wi-Fi password,
                                  which is why the site never serves it.
  ~/www/cgi-bin/nixie.cgi         the forms backend

  /usr/local/lib/liblogger.so     log shared by the three programs
  /usr/local/sbin/lighttpd        the web server

  /usr/local/bin/check_wifi_or_hotspot.sh   picks Wi-Fi or hotspot at boot
                                            (and again when the home network
                                            shows up), then starts lighttpd
  /usr/local/bin/check_ssid.sh              keeps looking for the home network

  /etc/systemd/system/            nixie, camera, wifi-check, check_ssid and
                                  lighttpd-custom

  ~/sources/clock/                the C++ sources (see "Building right
                                  here", below)
  ~/projeto/hardware/             schematics, board layout, panel
  ~/projeto/hardware/datasheet/   74141, 74HC595, IN-8, IN-9, K155ID1
  ~/projeto/fotos/                build photos and videos

lighttpd-custom.service is OFF on purpose: check_wifi_or_hotspot.sh is what
starts lighttpd. Turning both on puts two instances on port 80. hostapd and
dnsmasq are off at boot for the same reason: the script starts them when
needed. Disabled, not masked. Mask them and not even the script can start
them.


===============================================================================
 POKING AROUND THE GUTS
===============================================================================

Stop, start, check and restart nixie and camera:

  ~/nixiepi/services.sh {enable|disable|status|restart}

Edited ~/www/json/nixie.json by hand? Run ~/nixiepi/services.sh restart.
The site tells the clock on its own, the terminal does not.


New binaries built on a PC
--------------------------
Copy nixie, camera, nixie.cgi and liblogger.so to
~/sources/clock/dockerbuild/ (where the PC's rsync points) and run:

  ~/nixiepi/update_bins.sh

It checks that all four files are there BEFORE stopping anything, copies
them, runs ldconfig and restarts the services. A half-done copy won't take
the clock down.


Building right here
-------------------
The sources are in ~/sources/clock. The compiler only comes installed if the
card was built with --with-devtools. To find out:

  which cmake

If it says nothing, install it. It needs internet, meaning the clock on the
home network: in hotspot mode apt gets nowhere. About 1.2 GB.

  sudo apt-get update
  sudo apt-get install -y build-essential cmake pkg-config git \
                          libopencv-dev libcurl4-openssl-dev libpigpio-dev

Optional, to generate the code documentation:

  sudo apt-get install -y doxygen graphviz
  cd ~/sources/clock && doxygen Doxyfile

Output goes to ~/sources/clock/docs/html. There is no browser here, so take
it to another machine and open index.html:

  scp -r pi@nixie.local:sources/clock/docs/html nixie-docs

Building and installing:

  cd ~/sources/clock
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  make -C build -j1
  BUILD_DIR=~/sources/clock/build ~/nixiepi/update_bins.sh

The targets are "nixie", "camera", "nixie.cgi" and "logger". Easy as falling
off a log. But slow: the Raspberry Pi Zero has a single core. Go make a
coffee or binge your favorite series - all 12 seasons.


===============================================================================

Hardware, sources and photos are all here on the card. The only thing left
out is the tool that builds the card, because it runs on a PC. That one lives
in the clock's repository.

Good luck!!!!

Luiz Cressoni - luiz@cressoni.com.br
