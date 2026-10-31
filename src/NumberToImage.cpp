/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2014 James Packer

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY Or FITNESS For A PARTICULAR PURPOSE.  See the
     GNU General Public License For more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include "NumberToImage.hpp"
#include <vector>

//using namespace irr;

namespace NumberToImage
{

    const irr::u32 PADDING_PX = 1;
    const irr::video::SColor BG_COLOUR = irr::video::SColor(0,0,0,0);

    namespace
    {
        irr::video::IImage* digits[10] = { 0 };
    }

    void preload(irr::IrrlichtDevice* dev)
    {
        if (!dev) { return; }
        for (int d = 0; d < 10; d++) {
            if (digits[d]) { continue; }
            irr::io::path imagePath = "media/Char";
            imagePath.append((char)('0' + d));
            imagePath += ".png";
            digits[d] = dev->getVideoDriver()->createImageFromFile(imagePath); //kept for the whole run
        }
    }

    void draw(irr::u32 number, irr::video::IImage* target, irr::core::position2d<irr::s32> topLeft)
    {
        if (!target) { return; }
        const irr::core::stringc numberString = irr::core::stringc(number);
        irr::s32 x = topLeft.X;
        for (irr::u32 c = 0; c < numberString.size(); c++) {
            const int d = numberString.c_str()[c] - '0';
            irr::video::IImage* digit = (d >= 0 && d <= 9) ? digits[d] : 0;
            if (!digit) { continue; }
            const irr::core::dimension2du size = digit->getDimension();
            digit->copyToWithAlpha(target, irr::core::position2d<irr::s32>(x, topLeft.Y),
                irr::core::rect<irr::s32>(0, 0, (irr::s32)size.Width, (irr::s32)size.Height), irr::video::SColor(255, 255, 255, 255));
            x += (irr::s32)(size.Width + PADDING_PX);
        }
    }

    irr::video::IImage* getImage(irr::u32 number, irr::IrrlichtDevice* dev)
    {

        irr::core::stringc numberString = irr::core::stringc(number);
        const irr::u32 length = numberString.size();
        //irr::u32 imageWidth = length * CHAR_WIDTH;
        //irr::u32 imageHeight = CHAR_HEIGHT;

        if (length > 0) {

			std::vector<irr::video::IImage*> numberImages;
			numberImages.resize(length);
            //video::IImage* numberImages[length];
            irr::u32 overallWidth = 0;
            irr::u32 maxHeight = 0;
            //video::IImage* numberImage = dev->getVideoDriver()->createImage(video::ECF_R8G8B8, irr::core::dimension2d<irr::u32>(imageWidth, imageHeight));

            for (irr::u32 character = 0; character<length; character++) {
                //Load character image from file (charName.png)
                char thisChar = numberString.c_str()[character];

                irr::io::path imagePath = "media/Char";
                imagePath.append(thisChar);
                imagePath += ".png";

                numberImages[character] = dev->getVideoDriver()->createImageFromFile(imagePath);
                if (numberImages[character]) {
                    overallWidth+= numberImages[character]->getDimension().Width + PADDING_PX; //Padding at end
                    if (numberImages[character]->getDimension().Height > maxHeight) {
                        maxHeight = numberImages[character]->getDimension().Height;
                    }
                }


            }

            if (overallWidth>0) {
            	irr::video::IImage* numberImage = dev->getVideoDriver()->createImage(irr::video::ECF_A8R8G8B8, irr::core::dimension2d<irr::u32>(overallWidth, maxHeight));
                if (!numberImage) {
                    return 0;
                }

                numberImage->fill(BG_COLOUR); //Transparent
                //paste in the characters
                irr::u32 nextXStart = 0;
                for (irr::u32 character = 0; character<length; character++) {
                    if (numberImages[character]) {
                        irr::core::rect<irr::s32> sourceRect = irr::core::rect<irr::s32>(0,0,numberImages[character]->getDimension().Width,numberImages[character]->getDimension().Height);
                        numberImages[character]->copyToWithAlpha(numberImage,irr::core::vector2d<irr::s32>(nextXStart,0),sourceRect,irr::video::SColor(255,255,255,255));
                        nextXStart += numberImages[character]->getDimension().Width + PADDING_PX;
                        numberImages[character]->drop();
                    }
                }
                return numberImage;
            } else {
                return 0;
            }

        } else {
            return 0;
        }

    }

}

