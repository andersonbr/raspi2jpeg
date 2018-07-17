# raspi2jpeg

Utility to take a snapshot of the raspberry pi screen and save it as a JPEG file.

Based on raspi2png (https://github.com/AndrewFromMelbourne/raspi2png) utility.

    Usage: raspi2jpeg [--jpegname name] [--width <width>] [--height <height>] [--compression <level>] [--delay <delay>] [--display <number>] [--stdout] [--help]

    --jpegname,-j - name of JPEG file to create (default is snapshot.jpg)
    --height,-h - image height (default is screen height)
    --width,-w - image width (default is screen width)
    --compression,-c - JPEG compression level (0 - 9)
    --delay,-d - delay in seconds (default 0)
    --display,-D - Raspberry Pi display number (default 0)
	--stdout,-s - write file to stdout
    --help,-H - print this usage information

## Simple Install

Run this command through terminal or CLI screen.

curl -sL https://raw.githubusercontent.com/andersonbr/raspi2jpeg/master/installer.sh | bash -

## Manual Building

You will need to install libjpeg62-turbo-dev before you build the program. On Raspbian

sudo apt-get install -y libjpeg62-turbo-dev

Then just type 'make' in the raspi2jpeg directory you cloned from github.

