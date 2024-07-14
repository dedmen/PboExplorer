module;

//#include "lib/MipMapTool/TextureFile.hpp"
//#include "lib/s3tc-dxt-decompression/s3tc.h"
#include "lib/minilzo/minilzo.h"
#include <mutex>

#include "lib/MipMapTool/TextureFile.cpp"
#include "lib/s3tc-dxt-decompression/s3tc.cpp"
extern "C" {
#include "lib/minilzo/minilzo.c"
}

export module MipMapTool;



export class MipMapTool {
public:
    static std::tuple<std::vector<char>, int, int> LoadRGBATexture(std::istream& is, uint16_t maxSize = 4096) {

        TextureFile texFile;
        texFile.doLogging = false;

        texFile.readFromStream(is);

        if (texFile.mipmaps.empty())
            return {}; // ??? Happened once, probably corrupted mod. paaData was 26k of null bytes

        //Find biggest UNCOMPRESSED Mip
        auto biggestMip = *std::max_element(texFile.mipmaps.begin(), texFile.mipmaps.end(), [maxSize](const std::shared_ptr<MipMap>& l, const std::shared_ptr<MipMap>& r)
            {
                auto lSize = l->getRealSize();
                auto rSize = r->getRealSize();
                if (lSize > maxSize) lSize = 0;
                if (rSize > maxSize) rSize = 0;
                return lSize < rSize;
            });

        std::vector<char> output;
        uint32_t width = biggestMip->getRealSize();
        uint32_t height = biggestMip->height;
        output.resize(width * height * 4);

        std::vector<char>* DXTData = &biggestMip->data;
        std::vector<char> decompress;

        if (biggestMip->isCompressed()) {
            uint32_t expectedResultSize = width * height;
            if (texFile.type == PAAType::DXT1)
                expectedResultSize /= 2;

            lzo_uint dataSize = expectedResultSize;

            decompress.resize(expectedResultSize);
            lzo1x_decompress(reinterpret_cast<unsigned char*>(biggestMip->data.data()), biggestMip->data.size(), reinterpret_cast<unsigned char*>(decompress.data()), &dataSize, nullptr);
            if (dataSize != expectedResultSize)
                decompress.resize(dataSize);
            DXTData = &decompress;
        }


        if (texFile.type == PAAType::DXT5) {
            BlockDecompressImageDXT5(width, height, reinterpret_cast<unsigned char*>(DXTData->data()), reinterpret_cast<unsigned long*>(output.data()));
        }
        else if (texFile.type == PAAType::DXT1) {
            BlockDecompressImageDXT1(width, height, reinterpret_cast<unsigned char*>(DXTData->data()), reinterpret_cast<unsigned long*>(output.data()));
        }

        return { output, width, height };
    }
};

