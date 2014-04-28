See http://wiki.epfl.ch/line

mkdir build-line-gui
cd build-line-gui
qmake ../line-gui/line-gui.pro -spec linux-g++-64 CONFIG+=debug CONFIG+=declarative_debug
make -j4
cd ..

mkdir build-line-runner
cd build-line-runner
qmake ../line-runner/line-runner.pro -spec linux-g++-64 CONFIG+=debug CONFIG+=declarative_debug
make -j4
cd ..

cd line-router
./make-remote.sh
cd ..

cd line-traffic
./make-remote.sh
cd ..

recordmydesktop --width 1920 --height 1080 --channels 1 --device plughw:0,0 --fps 12 --v_quality 63 --s_quality 10 --v_bitrate 2000000 -o capture.ogv --no-frame --delay 5

