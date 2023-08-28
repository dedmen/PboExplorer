# PboExplorer

PboExplorer is a Windows explorer integration to provide PBO support. It works like zip files but for PBO's!


# Installation

1. Download the latest [Release](https://github.com/dedmen/PboExplorer/releases). Use the AIO zip file unless you know how to install manually.
2. Unpack the files into some directory that will be permanent. For example create a folder in %appdata% or in %programfiles% and place all files in there.
3. Run "register.cmd" as Administrator. This will register PboExplorer with the windows explorer.

Installing will overwrite already configured "open with" options for .pbo files.
There should be no need to restart explorer.


# Features

## Exploring PBO's

You can open PBO files by simply double-clicking them and then browse files as if it were just a zip folder.

https://github.com/dedmen/PboExplorer/assets/3768165/a6c00b2f-2ffb-4af6-8b0f-d9cd6239cf53

## Integration with Various tools

PboExplorer tries to be a one-stop-shop and integrates with common modding tools like Arma 3 Tools and Mikero's Tools.

For UI based tools it even tries to pre-fill the UI values, like in Addon Builder or Mikero's PboProject

https://github.com/dedmen/PboExplorer/assets/3768165/ed8ae97a-9015-4d69-a552-b6aafaf2e923

## Viewing PBO properties

(Still WIP Feature)
The file properties have a Tab for PboExplorer, which currently only shows the PBO Properties as a (editable!) list.

![explorer_2023-08-28_17-41-00](https://github.com/dedmen/PboExplorer/assets/3768165/6a11e9e4-a9e3-4ad0-b5bf-a8ff4b7c8938)

https://github.com/dedmen/PboExplorer/assets/3768165/674e0495-ba4a-425d-9d7b-6e426a2c3aba

## Quick Editing

PboExplorer not only allows you to view PBO's, it also allows you to edit them in-place.
One example is editing the PBO Properties above. But you can quickly open a PBO and Add/Remove/Modify files inside it, without fully unpacking/repacking it.
PBOExplorer will patch the PBO with minimal changes, so it even is very fast on large PBO files.

(This feature is NOT compatible with bisigning. Signing PBO's will require a full repack or defragmentation (not implemented yet), otherwise the dummy/temporary files created inside the PBO will mess up signatures)

https://github.com/dedmen/PboExplorer/assets/3768165/eae19ac3-dbfd-4763-86f5-75f4f585ab5e




