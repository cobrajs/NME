#include <Font.h>
#include <Utils.h>
#include <map>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#ifdef ANDROID
#include <android/log.h>
#endif


namespace nme
{

FT_Library sgLibrary = 0;


class FreeTypeFont : public FontFace
{
public:
   FreeTypeFont(FT_Face inFace, int inPixelHeight, int inTransform) :
     mFace(inFace), mPixelHeight(inPixelHeight),mTransform(inTransform)
   {
   }


   ~FreeTypeFont()
   {
      FT_Done_Face(mFace);
   }

   bool LoadBitmap(int inChar)
   {
      int idx = FT_Get_Char_Index( mFace, inChar );
      int err = FT_Load_Glyph( mFace, idx, FT_LOAD_DEFAULT  );
      if (err)
         return false;

      FT_Render_Mode mode = FT_RENDER_MODE_NORMAL;
      // mode = FT_RENDER_MODE_MONO;
      if (mFace->glyph->format != FT_GLYPH_FORMAT_BITMAP)
         err = FT_Render_Glyph( mFace->glyph, mode );
      if (err)
         return false;

		#ifndef GPH
      if (mTransform & ffBold)
      {
         FT_GlyphSlot_Own_Bitmap(mFace->glyph);
         FT_Bitmap_Embolden(sgLibrary, &mFace->glyph->bitmap, 1<<6, 0);
      }
		#endif
      return true;
   }


   bool GetGlyphInfo(int inChar, int &outW, int &outH, int &outAdvance,
                           int &outOx, int &outOy)
   {
      if (!LoadBitmap(inChar))
         return false;

      outOx = mFace->glyph->bitmap_left;
      outOy = -mFace->glyph->bitmap_top;
      FT_Bitmap &bitmap = mFace->glyph->bitmap;
      outW = bitmap.width;
      outH = bitmap.rows;
      outAdvance = (mFace->glyph->advance.x >> 6);
      return true;
   }


   void RenderGlyph(int inChar,const RenderTarget &outTarget)
   {
      if (!LoadBitmap(inChar))
         return;

      FT_Bitmap &bitmap = mFace->glyph->bitmap;
      int w = bitmap.width;
      int h = bitmap.rows;
      if (w>outTarget.mRect.w || h>outTarget.mRect.h)
         return;

      for(int r=0;r<h;r++)
      {
         unsigned char *row = bitmap.buffer + r*bitmap.pitch;
         uint8  *dest = (uint8 *)outTarget.Row(r + outTarget.mRect.y) + outTarget.mRect.x;

         if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
         {
            int bit = 0;
            int data = 0;
            for(int x=0;x<outTarget.mRect.w;x++)
            {
               if (!bit)
               {
                  bit = 128;
                  data = *row++;
               }
               *dest++ =  (data & bit) ? 0xff: 0x00;
               bit >>= 1;
            }
         }
         else if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
         {
            for(int x=0;x<w;x++)
               *dest ++ = *row++;
         }
      }
   }


   int Height()
   {
      return mFace->size->metrics.height/(1<<6);
   }


   void UpdateMetrics(TextLineMetrics &ioMetrics)
   {
      if (mFace)
      {
         FT_Size_Metrics &metrics = mFace->size->metrics;
         ioMetrics.ascent = std::max( ioMetrics.ascent, (float)metrics.ascender/(1<<6) );
         ioMetrics.descent = std::max( ioMetrics.descent, (float)metrics.descender/(1<<6) );
         ioMetrics.height = std::max( ioMetrics.height, (float)metrics.height/(1<<6) );
      }
   }


   FT_Face  mFace;
   uint32 mTransform;
   int    mPixelHeight;

};


static FT_Face OpenFont(const std::string &inFace, unsigned char inFlags)
{
   FT_Face face = 0;
   FT_New_Face(sgLibrary, inFace.c_str(), 0, &face);
   if (face && inFlags!=0 && face->num_faces>1)
   {
      int n = face->num_faces;
      // Look for other font that may match
      for(int f=1;f<n;f++)
      {
         FT_Face test = 0;
         FT_New_Face(sgLibrary, inFace.c_str(), f, &test);
         if (test && test->style_flags == inFlags)
         {
            // A goodie!
            FT_Done_Face(face);
            return test;
         }
         else if (test)
            FT_Done_Face(test);
      }
      // The original face will have to do...
   }
   return face;
}






#ifdef HX_WINDOWS
#include <windows.h>
#include <tchar.h>

#define strcasecmp stricmp

bool GetFontFile(const std::string& inName,std::string &outFile)
{
   _TCHAR win_path[2 * MAX_PATH];
   GetWindowsDirectory(win_path, 2*MAX_PATH);
   outFile = std::string(win_path) + "\\Fonts\\" + inName;

   return true;
}


#elif defined(GPH)

bool GetFontFile(const std::string& inName,std::string &outFile)
{
   outFile = "/usr/gp2x/HYUni_GPH_B.ttf";
   return true;
}

#elif defined(__APPLE__)
bool GetFontFile(const std::string& inName,std::string &outFile)
{

#ifdef IPHONEOS
#define FONT_BASE "/System/Library/Fonts/Cache/"
#define TIMES_ROMAN "TimesNewRoman.ttf"
#else
#define FONT_BASE "/Library/Fonts/"
#define TIMES_ROMAN "Times New Roman.ttf"
#endif

   if (!strcasecmp(inName.c_str(),"times.ttf"))
      outFile = FONT_BASE TIMES_ROMAN;
   else if (!strcasecmp(inName.c_str(),"arial.ttf"))
      outFile = FONT_BASE "Arial.ttf";
   else if (!strcasecmp(inName.c_str(),"courier.ttf"))
      outFile = FONT_BASE "Courier.ttf";
   else if (!strcasecmp(inName.c_str(),"helvetica.ttf"))
      outFile = FONT_BASE "Helvetica.ttf";
   else
   {
      outFile = FONT_BASE + inName;
      return true;
      //printf("Unfound font: %s\n",inName.c_str());
      return false;
   }

   return true;
}
#else

bool GetFontFile(const std::string& inName,std::string &outFile)
{
   #ifdef ANDROID
   if (!strcasecmp(inName.c_str(),"times.ttf"))
      outFile = "/system/fonts/DroidSerif-Regular.ttf";
   else if (!strcasecmp(inName.c_str(),"arial.ttf"))
      outFile = "/system/fonts/DroidSans.ttf";
   else if (!strcasecmp(inName.c_str(),"courier.ttf"))
      outFile = "/system/fonts/SansMono.ttf";
   #else
   if (!strcasecmp(inName.c_str(),"times.ttf"))
      outFile = "/usr/share/fonts/truetype/freefont/FreeSerif.ttf";
   else if (!strcasecmp(inName.c_str(),"arial.ttf"))
      outFile = "/usr/share/fonts/truetype/freefont/FreeSans.ttf";
   else if (!strcasecmp(inName.c_str(),"courier.ttf"))
      outFile = "/usr/share/fonts/truetype/freefont/FreeMono.ttf";
   #endif
   else
   {
       #ifdef ANDROID
       __android_log_print(ANDROID_LOG_INFO, "GetFontFile", "Could not load font %s.",
          inName.c_str() );
       #endif

      //printf("Unfound font: %s\n",inName.c_str());
      return false;
   }

   return true;
}
#endif



FT_Face FindFont(const std::string &inFontName, unsigned int inFlags)
{
   std::string fname = inFontName;
   if (fname.find(".")==std::string::npos)
      fname += ".ttf";

   FT_Face font = OpenFont(fname,inFlags);

   if (font==0 && fname.find("\\")==std::string::npos && fname.find("/")==std::string::npos)
   {
      std::string file_name;
      if (font==0)
         font = OpenFont(("./fonts/" + fname).c_str(),inFlags);

      if (font==0)
         font = OpenFont(("./data/" + fname).c_str(),inFlags);

      if (font==0 && GetFontFile(fname,file_name))
      {
         // printf("Found font in %s\n", file_name.c_str());
         font = OpenFont(file_name.c_str(),inFlags);

			// printf("Opened : %p\n", font);
      }
   }


   return font;
}




FontFace *FontFace::CreateFreeType(const TextFormat &inFormat,double inScale)
{
   if (!sgLibrary)
     FT_Init_FreeType( &sgLibrary );
   if (!sgLibrary)
      return 0;

   FT_Face face = 0;
   std::string str = WideToUTF8(inFormat.font);

   uint32 flags = 0;
   if (inFormat.bold)
      flags |= ffBold;
   if (inFormat.italic)
      flags |= ffItalic;

   face = FindFont(str,flags);
   if (!face)
      return 0;

   int height = (int )(inFormat.size*inScale + 0.5);
   FT_Set_Pixel_Sizes(face,0, height);


   uint32 transform = 0;
   if ( !(face->style_flags & ffBold) && inFormat.bold )
      transform |= ffBold;
   if ( !(face->style_flags & ffItalic) && inFormat.italic )
      transform |= ffItalic;
   return new FreeTypeFont(face,height,transform);
}





} // end namespace nme

#include <hx/CFFI.h>

// Font outlines as data - from the SamHaXe project

#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

namespace {
enum {
   PT_MOVE = 1,
   PT_LINE = 2,
   PT_CURVE = 3
};

struct point {
   int            x, y;
   unsigned char  type;

   point() { }
   point(int x, int y, unsigned char type) : x(x), y(y), type(type) { }
};

struct glyph {
   FT_ULong                char_code;
   FT_Vector               advance;
   FT_Glyph_Metrics        metrics;
   int                     index, x, y;
   std::vector<int>        pts;

   glyph(): x(0), y(0) { }
};

struct kerning {
   int                     l_glyph, r_glyph;
   int                     x, y;

   kerning() { }
   kerning(int l, int r, int x, int y): l_glyph(l), r_glyph(r), x(x), y(y) { }
};

struct glyph_sort_predicate {
   bool operator()(const glyph* g1, const glyph* g2) const {
      return g1->char_code <  g2->char_code;
   }
};

#ifdef GPH
typedef FT_Vector *FVecPtr;
#else
typedef const FT_Vector *FVecPtr;
#endif

int outline_move_to(FVecPtr to, void *user) {
   glyph       *g = static_cast<glyph*>(user);

   g->pts.push_back(PT_MOVE);
   g->pts.push_back(to->x);
   g->pts.push_back(to->y);

   g->x = to->x;
   g->y = to->y;
   
   return 0;
}

int outline_line_to(FVecPtr to, void *user) {
   glyph       *g = static_cast<glyph*>(user);

   g->pts.push_back(PT_LINE);
   g->pts.push_back(to->x - g->x);
   g->pts.push_back(to->y - g->y);
   
   g->x = to->x;
   g->y = to->y;
   
   return 0;
}

int outline_conic_to(FVecPtr ctl, FVecPtr to, void *user) {
   glyph       *g = static_cast<glyph*>(user);

   g->pts.push_back(PT_CURVE);
   g->pts.push_back(ctl->x - g->x);
   g->pts.push_back(ctl->y - g->y);
   g->pts.push_back(to->x - ctl->x);
   g->pts.push_back(to->y - ctl->y);
   
   g->x = to->x;
   g->y = to->y;
   
   return 0;
}

int outline_cubic_to(FVecPtr, FVecPtr , FVecPtr , void *user) {
   // Cubic curves are not supported
   return 1;
}

} // end namespace

value freetype_init() {
   if (!nme::sgLibrary)
     FT_Init_FreeType( &nme::sgLibrary );

   return alloc_bool(nme::sgLibrary);
}

value freetype_import_font(value font_file, value char_vector, value em_size) {
   FT_Face           face;
   int               result, i, j;

   val_check(font_file, string);
   val_check(em_size, int);

   result = FT_New_Face(nme::sgLibrary, val_string(font_file), 0, &face);
     
   if (result == FT_Err_Unknown_File_Format) {
      val_throw(alloc_string("Unknown file format!"));
      return alloc_null();
   
   } else if(result != 0) {
      val_throw(alloc_string("File open error!"));
      return alloc_null();
   }

   if(!FT_IS_SCALABLE(face)) {
      FT_Done_Face(face);

      val_throw(alloc_string("Font is not scalable!"));
      return alloc_null();
   }

   int        em = val_int(em_size);
   FT_Set_Char_Size(face, em, em, 72, 72);

   std::vector<glyph*>     glyphs;

   FT_Outline_Funcs     ofn = {
      outline_move_to,
      outline_line_to,
      outline_conic_to,
      outline_cubic_to,
      0, // shift
      0  // delta
   };

   if(!val_is_null(char_vector)) {
      // Import only specified characters
      int         num_char_codes = val_array_size(char_vector);

      for(i = 0; i < num_char_codes; i++) {
         FT_ULong    char_code = (FT_ULong)val_int(val_array_i(char_vector,i));
         FT_UInt     glyph_index = FT_Get_Char_Index(face, char_code);

         if(glyph_index != 0 && FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) == 0) {
            glyph             *g = new glyph;

            result = FT_Outline_Decompose(&face->glyph->outline, &ofn, g);
            if(result == 0) {
               g->index = glyph_index;
               g->char_code = char_code;
               g->metrics = face->glyph->metrics;
               glyphs.push_back(g);
            } else
               delete g;
         }
      }

   } else {
      // Import every character in face
      FT_ULong    char_code;
      FT_UInt     glyph_index;

      char_code = FT_Get_First_Char(face, &glyph_index);
      while(glyph_index != 0) {
         if(FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) == 0) {
            glyph             *g = new glyph;

            result = FT_Outline_Decompose(&face->glyph->outline, &ofn, g);
            if(result == 0) {
               g->index = glyph_index;
               g->char_code = char_code;
               g->metrics = face->glyph->metrics;
               glyphs.push_back(g);
            } else
               delete g;
         }
         
         char_code = FT_Get_Next_Char(face, char_code, &glyph_index);  
      }
   }

   // Ascending sort by character codes
   std::sort(glyphs.begin(), glyphs.end(), glyph_sort_predicate());

   std::vector<kerning>      kern;
   if(FT_HAS_KERNING(face)) {
      int         n = glyphs.size();
      FT_Vector   v;

      for(i = 0; i < n; i++) {
         int      l_glyph = glyphs[i]->index;

         for(j = 0; j < n; j++) {
            int   r_glyph = glyphs[j]->index;

            FT_Get_Kerning(face, l_glyph, r_glyph, FT_KERNING_DEFAULT, &v);
            if(v.x != 0 || v.y != 0)
               kern.push_back( kerning(i, j, v.x, v.y) );
         }
      }
   }

   int               num_glyphs = glyphs.size();
   
   value             ret = alloc_empty_object();
   alloc_field(ret, val_id("has_kerning"), alloc_bool(FT_HAS_KERNING(face)));
   alloc_field(ret, val_id("is_fixed_width"), alloc_bool(FT_IS_FIXED_WIDTH(face)));
   alloc_field(ret, val_id("has_glyph_names"), alloc_bool(FT_HAS_GLYPH_NAMES(face)));
   alloc_field(ret, val_id("is_italic"), alloc_bool(face->style_flags & FT_STYLE_FLAG_ITALIC));
   alloc_field(ret, val_id("is_bold"), alloc_bool(face->style_flags & FT_STYLE_FLAG_BOLD));
   alloc_field(ret, val_id("num_glyphs"), alloc_int(num_glyphs));
   alloc_field(ret, val_id("family_name"), alloc_string(face->family_name));
   alloc_field(ret, val_id("style_name"), alloc_string(face->style_name));
   alloc_field(ret, val_id("em_size"), alloc_int(face->units_per_EM));
   alloc_field(ret, val_id("ascend"), alloc_int(face->ascender));
   alloc_field(ret, val_id("descend"), alloc_int(face->descender));
   alloc_field(ret, val_id("height"), alloc_int(face->height));

   // 'glyphs' field
   value             neko_glyphs = alloc_array(num_glyphs);
   for(i = 0; i < glyphs.size(); i++) {
      glyph          *g = glyphs[i];
      int            num_points = g->pts.size();

      value          points = alloc_array(num_points);
      
      for(j = 0; j < num_points; j++)
         val_array_set_i(points,j,alloc_int(g->pts[j]));

      value item = alloc_empty_object();
      val_array_set_i(neko_glyphs,i,item);
      alloc_field(item, val_id("char_code"), alloc_int(g->char_code));
      alloc_field(item, val_id("advance"), alloc_int(g->metrics.horiAdvance));
      alloc_field(item, val_id("min_x"), alloc_int(g->metrics.horiBearingX));
      alloc_field(item, val_id("max_x"), alloc_int(g->metrics.horiBearingX + g->metrics.width));
      alloc_field(item, val_id("min_y"), alloc_int(g->metrics.horiBearingY - g->metrics.height));
      alloc_field(item, val_id("max_y"), alloc_int(g->metrics.horiBearingY));
      alloc_field(item, val_id("points"), points);

      delete g;
   }
   alloc_field(ret, val_id("glyphs"), neko_glyphs);

   // 'kerning' field
   if(FT_HAS_KERNING(face)) {
      value       neko_kerning = alloc_array(kern.size());

      for(i = 0; i < kern.size(); i++) {
         kerning  *k = &kern[i];

         value item = alloc_empty_object();
         val_array_set_i(neko_kerning,i,item);
         alloc_field(item, val_id("left_glyph"), alloc_int(k->l_glyph));
         alloc_field(item, val_id("right_glyph"), alloc_int(k->r_glyph));
         alloc_field(item, val_id("x"), alloc_int(k->x));
         alloc_field(item, val_id("y"), alloc_int(k->y));
      }
      
      alloc_field(ret, val_id("kerning"), neko_kerning);
   } else
      alloc_field(ret, val_id("kerning"), alloc_null());

   FT_Done_Face(face);
   
   return ret;
}

DEFINE_PRIM(freetype_init, 0);
DEFINE_PRIM(freetype_import_font, 3);

