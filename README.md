# sf2generate

A minimal SF2 (SoundFont 2) writer for a single mono WAV file.
It aims to generate a SF2 SoundFont from a single audio file.
It could act as command-line application like the following

```shell
sf2generate input.wav output.sf2
```
That will generate a SF2 SoundFont with two instruments, 
a SingleShoot and a LOOP.
The base Note is set to 60 (C4).

When staring without command-line options it will start a GUI which 
allow you to select a audio file and set the loop points to be used 
for the looped instrument. When done you could save the generated Soundfont.

That's it, no more, no less.

The GUI is created with libxputty.

## Features

## Dependencies

- libsndfile1-dev
- portaudio19-dev
- libcairo2-dev
- libx11-dev

## Building from source code

```shell
git clone https://github.com/brummer10/sf2generate.git
cd sf2generate
git submodule update --init --recursive
make
sudo make install # will install into /usr/bin
```
