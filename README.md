# sf2generate

A minimal SF2 (SoundFont 2) writer for a single audio file.
Note, regardless ow many channels the file have, only the first one will be used.
It could act as command-line application like the following

```shell
sf2generate input.wav output.sf2 
```
That will generate a SF2 SoundFont with two instruments, 
a SingleShoot and a LOOP.
The base Note is set to 60 (C4).

Additional optional arguments been RootKey Chorus Reverb
They must been given in that order, when used.
Chorus and Reverb been percent in the range from 0 to 100
RootKey is the MIDI Note in the range from 0 to 127

Default settings been
```shell
 60 50 50
```
## Features

When starting without command-line arguments it will start a GUI which 
allow you to select a audio file,
clip it to the partial range to be used for the OneShoot Instrument,
set the loop points to be used for the Looped Instrument,
set the base Note and 
the values in percent for Chorus and Reverb. 
When done, the SoundFont could be generated and saved.

That's it, not more, not less.

The GUI is created with libxputty.


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
