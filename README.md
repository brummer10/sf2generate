# sf2generate

A minimal SF2 (SoundFont 2) writer for a single audio file.
Note, regardless how many channels the file have, only the first one will be used.
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

<p align="center">
    <img src="https://github.com/brummer10/sf2generate/blob/main/sf2generate.png?raw=true" />
</p>

When starting without command-line arguments it will start a GUI which 
allow you to select a audio file,
clip it to the range to be used for the OneShoot Instrument
and set the loop points to be used for the Looped Instrument.

<p align="center">
    <img src="https://github.com/brummer10/sf2generate/blob/main/sf2generate-settings.png?raw=true" />
</p>

A pitch detector will then calculate the RootKey and
the Pitch correction to be used in the SoundFont.
You could still change that to your needs, if you wish.

As well you could set the values in percent for Chorus and Reverb.

When done, the SoundFont could be generated and saved.

That's it, not more, not less.

The GUI is created with libxputty.


## Dependencies

- libsndfile1-dev
- libfftw3-dev
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
