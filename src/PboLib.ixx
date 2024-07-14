module;

#include "lib/ArmaPboLib/src/pbo.hpp"

export module PboLib;

extern "C++" {

    export class PboProperty;
    export enum class PboEntryPackingMethod;
    export class PboEntry;
    export class PboEntryBuffer;
    export class PboReader;

}
