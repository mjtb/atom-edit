# atom-edit

Windows command-line interface for the Atom (<https://atom.io/>) editor.

The source code in this repository is made available under a permissive MIT license. Please see the
LICENSE file for full details.


# Build Environment

*	Visual Studio 2015


# Runtime Environment

*	Windows 10
*	Atom


# Build Process

1.	Check out sources
2.	Build edit.sln


# Command Line Interface

`edit.exe file1 file2 ... fileN`

This will launch Atom with one or more files specified on the command-line.

`command | edit.exe`

This will pipe standard input to a temporary file and then launch Atom to edit the temporary file.
The temporary file is _not_ automatically deleted.

`edit.exe`

This will run the Atom auto-updater, which will then launch the application.

`edit.exe --wait file1 ...`

This will run the editor and wait for the Atom window to be closed. The wait-until-close behaviour can be made
default by renaming the program executable file to `edit-wait.exe`.

**N.B.**	The list of files to be opened upon startup contained in
`~/.atom/storage/application.js` is cleared every time the program runs.


# Open Source Licenses

This software uses the **JsonCpp library** (<https://github.com/open-source-parsers/jsoncpp>).
Copyright (c) 2007-2010 Baptiste Lepilleur.
Used under an MIT license.
