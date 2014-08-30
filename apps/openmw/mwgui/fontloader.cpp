#include "fontloader.hpp"

#include <OgreResourceGroupManager.h>
#include <OgreTextureManager.h>

#include <MyGUI_ResourceManager.h>
#include <MyGUI_FontManager.h>
#include <MyGUI_ResourceManualFont.h>
#include <MyGUI_FactoryManager.h>


#include <components/misc/stringops.hpp>

namespace
{
    unsigned long utf8ToUnicode(const std::string& utf8)
    {
        size_t i = 0;
        unsigned long unicode;
        size_t todo;
        unsigned char ch = utf8[i++];
        if (ch <= 0x7F)
        {
            unicode = ch;
            todo = 0;
        }
        else if (ch <= 0xBF)
        {
            throw std::logic_error("not a UTF-8 string");
        }
        else if (ch <= 0xDF)
        {
            unicode = ch&0x1F;
            todo = 1;
        }
        else if (ch <= 0xEF)
        {
            unicode = ch&0x0F;
            todo = 2;
        }
        else if (ch <= 0xF7)
        {
            unicode = ch&0x07;
            todo = 3;
        }
        else
        {
            throw std::logic_error("not a UTF-8 string");
        }
        for (size_t j = 0; j < todo; ++j)
        {
            unsigned char ch = utf8[i++];
            if (ch < 0x80 || ch > 0xBF)
                throw std::logic_error("not a UTF-8 string");
            unicode <<= 6;
            unicode += ch & 0x3F;
        }
        if (unicode >= 0xD800 && unicode <= 0xDFFF)
            throw std::logic_error("not a UTF-8 string");
        if (unicode > 0x10FFFF)
            throw std::logic_error("not a UTF-8 string");

        return unicode;
    }

    // getUtf8, aka the worst function ever written.
    // This includes various hacks for dealing with Morrowind's .fnt files that are *mostly*
    // in the expected win12XX encoding, but also have randomly swapped characters sometimes.
    // Looks like the Morrowind developers found standard encodings too boring and threw in some twists for fun.
    std::string getUtf8 (unsigned char c, ToUTF8::Utf8Encoder& encoder, ToUTF8::FromType encoding)
    {
        if (encoding == ToUTF8::WINDOWS_1250)
        {
            // Hacks for polish font
            unsigned char win1250;
            std::map<unsigned char, unsigned char> conv;
            conv[0x80] = 0xc6;
            conv[0x81] = 0x9c;
            conv[0x82] = 0xe6;
            conv[0x83] = 0xb3;
            conv[0x84] = 0xf1;
            conv[0x85] = 0xb9;
            conv[0x86] = 0xbf;
            conv[0x87] = 0x9f;
            conv[0x88] = 0xea;
            conv[0x89] = 0xea;
            conv[0x8a] = 0x0; // not contained in win1250
            conv[0x8b] = 0x0; // not contained in win1250
            conv[0x8c] = 0x8f;
            conv[0x8d] = 0xaf;
            conv[0x8e] = 0xa5;
            conv[0x8f] = 0x8c;
            conv[0x90] = 0xca;
            conv[0x93] = 0xa3;
            conv[0x94] = 0xf6;
            conv[0x95] = 0xf3;
            conv[0x96] = 0xaf;
            conv[0x97] = 0x8f;
            conv[0x99] = 0xd3;
            conv[0x9a] = 0xd1;
            conv[0x9c] = 0x0; // not contained in win1250
            conv[0xa0] = 0xb9;
            conv[0xa1] = 0xaf;
            conv[0xa2] = 0xf3;
            conv[0xa3] = 0xbf;
            conv[0xa4] = 0x0; // not contained in win1250
            conv[0xe1] = 0x8c;
            // Can't remember if this was supposed to read 0xe2, or is it just an extraneous copypaste?
            //conv[0xe1] = 0x8c;
            conv[0xe3] = 0x0; // not contained in win1250
            conv[0xf5] = 0x0; // not contained in win1250

            if (conv.find(c) != conv.end())
                win1250 = conv[c];
            else
                win1250 = c;
            return encoder.getUtf8(std::string(1, win1250));
        }
        else
            return encoder.getUtf8(std::string(1, c));
    }


    typedef struct
    {
        float x;
        float y;
    } Point;

    typedef struct
    {
        float u1; // appears unused, always 0
        Point top_left;
        Point top_right;
        Point bottom_left;
        Point bottom_right;
        float width;
        float height;
        float u2; // appears unused, always 0
        float kerning;
        float ascent;
    } GlyphInfo;

    void addFontGlyph(MyGUI::ResourceManualFont* font, MyGUI::Char unicodeVal, GlyphInfo& data, int fontSize)
    {
        float x1 = data.top_left.x;
        float y1 = data.top_left.y;
        float x2 = data.top_right.x;
        float y2 = data.bottom_left.y;
        font->addGlyphInfo(unicodeVal,
                           MyGUI::GlyphInfo(unicodeVal, data.width, data.height,
                                            data.width, data.kerning, (fontSize - data.ascent),
                           MyGUI::FloatRect(x1, y1, x2, y2)));
    }

}

namespace MWGui
{

    FontLoader::FontLoader(ToUTF8::FromType encoding)
    {
        if (encoding == ToUTF8::WINDOWS_1252)
            mEncoding = ToUTF8::CP437;
        else
            mEncoding = encoding;
    }

    void FontLoader::loadAllFonts(bool exportToFile)
    {
        Ogre::StringVector groups = Ogre::ResourceGroupManager::getSingleton().getResourceGroups ();
        for (Ogre::StringVector::iterator it = groups.begin(); it != groups.end(); ++it)
        {
            Ogre::StringVectorPtr resourcesInThisGroup = Ogre::ResourceGroupManager::getSingleton ().findResourceNames (*it, "*.fnt");
            for (Ogre::StringVector::iterator resource = resourcesInThisGroup->begin(); resource != resourcesInThisGroup->end(); ++resource)
            {
                loadFont(*resource, exportToFile);
            }
        }
    }

    void FontLoader::loadFont(const std::string &fileName, bool exportToFile)
    {
        Ogre::DataStreamPtr file = Ogre::ResourceGroupManager::getSingleton().openResource(fileName);

        float fontSize;
        int one;
        file->read(&fontSize, sizeof(fontSize));

        file->read(&one, sizeof(int));
        assert(one == 1);
        file->read(&one, sizeof(int));
        assert(one == 1);

        char name_[284];
        file->read(name_, sizeof(name_));
        std::string name(name_);

        GlyphInfo data[256];
        file->read(data, sizeof(data));
        file->close();

        // Create the font texture
        std::string bitmapFilename = "Fonts/" + std::string(name) + ".tex";
        Ogre::DataStreamPtr bitmapFile = Ogre::ResourceGroupManager::getSingleton().openResource(bitmapFilename);

        int width, height;
        bitmapFile->read(&width, sizeof(int));
        bitmapFile->read(&height, sizeof(int));

        std::vector<Ogre::uchar> textureData;
        textureData.resize(width*height*4);
        bitmapFile->read(&textureData[0], width*height*4);
        bitmapFile->close();

        std::string resourceName;
        if (name.size() >= 5 && Misc::StringUtils::ciEqual(name.substr(0, 5), "magic"))
            resourceName = "Magic Cards";
        else if (name.size() >= 7 && Misc::StringUtils::ciEqual(name.substr(0, 7), "century"))
            resourceName = "Century Gothic";
        else if (name.size() >= 7 && Misc::StringUtils::ciEqual(name.substr(0, 7), "daedric"))
            resourceName = "Daedric";
        else
            return; // no point in loading it, since there is no way of using additional fonts

        std::string textureName = name;
        Ogre::Image image;
        image.loadDynamicImage(&textureData[0], width, height, Ogre::PF_BYTE_RGBA);
        Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().createManual(textureName,
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TEX_TYPE_2D,
            width, height, 0, Ogre::PF_BYTE_RGBA);
        texture->loadImage(image);

        if (exportToFile)
            image.save(resourceName + ".png");

        // Register the font with MyGUI
        MyGUI::ResourceManualFont* font = static_cast<MyGUI::ResourceManualFont*>(
                    MyGUI::FactoryManager::getInstance().createObject("Resource", "ResourceManualFont"));

        font->setDefaultHeight(fontSize);
        font->setSource(textureName);
        font->setResourceName(resourceName);

        for(int i = 0; i < 256; i++)
        {
            ToUTF8::Utf8Encoder encoder(mEncoding);
            unsigned long unicodeVal = utf8ToUnicode(getUtf8(i, encoder, mEncoding));

            addFontGlyph(font, unicodeVal, data[i], fontSize);

            // More hacks! The french game uses several win1252 characters that are not included
            // in the cp437 encoding of the font. Fall back to similar available characters.
            if (mEncoding == ToUTF8::CP437)
            {
                std::multimap<int, int> additional; // <cp437, unicode>
                additional.insert(std::make_pair(39, 0x2019)); // apostrophe
                additional.insert(std::make_pair(45, 0x2013)); // dash
                additional.insert(std::make_pair(45, 0x2014)); // dash
                additional.insert(std::make_pair(34, 0x201D)); // right double quotation mark
                additional.insert(std::make_pair(34, 0x201C)); // left double quotation mark
                additional.insert(std::make_pair(44, 0x201A));
                additional.insert(std::make_pair(44, 0x201E));
                additional.insert(std::make_pair(43, 0x2020));
                additional.insert(std::make_pair(94, 0x02C6));
                additional.insert(std::make_pair(37, 0x2030));
                additional.insert(std::make_pair(83, 0x0160));
                additional.insert(std::make_pair(60, 0x2039));
                additional.insert(std::make_pair(79, 0x0152));
                additional.insert(std::make_pair(90, 0x017D));
                additional.insert(std::make_pair(39, 0x2019));
                additional.insert(std::make_pair(126, 0x02DC));
                additional.insert(std::make_pair(84, 0x2122));
                additional.insert(std::make_pair(83, 0x0161));
                additional.insert(std::make_pair(62, 0x203A));
                additional.insert(std::make_pair(111, 0x0153));
                additional.insert(std::make_pair(122, 0x017E));
                additional.insert(std::make_pair(89, 0x0178));
                additional.insert(std::make_pair(156, 0x00A2));
                additional.insert(std::make_pair(46, 0x2026));

                for (std::multimap<int, int>::iterator it = additional.begin(); it != additional.end(); ++it)
                {
                    if (it->first != i)
                        continue;

                    addFontGlyph(font, it->second, data[i], fontSize);
                }
            }

            // ASCII vertical bar, use this as text input cursor
            if (i == 124)
            {
                addFontGlyph(font, MyGUI::FontCodeType::Cursor, data[i], fontSize);
            }

            // Question mark, use for NotDefined marker (used for glyphs not existing in the font)
            if (i == 63)
            {
                addFontGlyph(font, MyGUI::FontCodeType::NotDefined, data[i], fontSize);
            }
        }

        // These are required as well, but the fonts don't provide them
        for (int i=0; i<2; ++i)
        {
            MyGUI::FontCodeType::Enum type;
            if(i == 0)
                type = MyGUI::FontCodeType::Selected;
            else if (i == 1)
                type = MyGUI::FontCodeType::SelectedBack;

            GlyphInfo empty;
            Point emptyPoint;
            emptyPoint.x = 0;
            emptyPoint.y = 0;
            empty.ascent = 0;
            empty.bottom_left = emptyPoint;
            empty.bottom_right = emptyPoint;
            empty.height = 0;
            empty.kerning = 0;
            empty.top_left = emptyPoint;
            empty.top_right = emptyPoint;
            empty.u1 = 0;
            empty.u2 = 0;
            empty.width = 0;
            addFontGlyph(font, type, empty, fontSize);
        }

        MyGUI::ResourceManager::getInstance().removeByName(font->getResourceName());
        MyGUI::ResourceManager::getInstance().addResource(font);
    }

}
