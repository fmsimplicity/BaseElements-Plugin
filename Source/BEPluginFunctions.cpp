/*
 BEPluginFunctions.cpp
 BaseElements Plug-In
 
 Copyright 2010-2011 Goya. All rights reserved.
 For conditions of distribution and use please see the copyright notice in BEPlugin.cpp
 
 http://www.goya.com.au/baseelements/plugin
 
 */


#include "BEPluginGlobalDefines.h"
#include "BEPluginFunctions.h"
#include "BEXSLT.h"
#include "BEWStringVector.h"
#include "BECurl.h"
#include "BEMessageDigest.h"
#include "BEFileSystem.h"
#include "BEShell.h"
#include "BEZlib.h"
#include "BESQLCommand.h"


#if defined(FMX_WIN_TARGET)

	#include "afxdlgs.h"
	#include <locale.h>

	#include "resource.h"
	#include "BEWinFunctions.h"

#endif

#if defined(FMX_MAC_TARGET)

	#include "BEMacFunctions.h"

#endif

#include "FMWrapper/FMXFixPt.h"
#include "FMWrapper/FMXData.h"
#include "FMWrapper/FMXBinaryData.h"
#include "FMWrapper/FMXCalcEngine.h"

#include "boost/filesystem.hpp"
#include "boost/filesystem/fstream.hpp"
#include "boost/thread.hpp"
#include "boost/foreach.hpp"
#include "boost/tokenizer.hpp"
#include "boost/algorithm/string.hpp"

#include "boost/archive/iterators/base64_from_binary.hpp"
#include "boost/archive/iterators/binary_from_base64.hpp"
#include "boost/archive/iterators/transform_width.hpp"
#include "boost/archive/iterators/insert_linebreaks.hpp"
#include "boost/archive/iterators/remove_whitespace.hpp"


#include <iostream>


using namespace std;
using namespace boost::filesystem;
using namespace boost::archive::iterators;


typedef insert_linebreaks<
	base64_from_binary<
		transform_width<char *, 6, 8>
	> ,72
> base64_text;

typedef transform_width<
	binary_from_base64<
		remove_whitespace<string::const_iterator>
	>, 8, 6
> base64_binary;



#pragma mark -
#pragma mark Globals
#pragma mark -

errcode g_last_error;
errcode g_last_ddl_error;
string g_text_encoding;


extern int g_http_response_code;
extern string g_http_response_headers;
extern CustomHeaders g_http_custom_headers;


#pragma mark -
#pragma mark Version
#pragma mark -

FMX_PROC(errcode) BE_Version ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& /* parameters */, Data& results)
{
	return TextConstantFunction ( VERSION_NUMBER_STRING, results );	
}


FMX_PROC(errcode) BE_VersionAutoUpdate ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& /* parameters */, Data& results)
{
	return TextConstantFunction ( AUTO_UPDATE_VERSION, results );		
}


#pragma mark -
#pragma mark Errors
#pragma mark -


FMX_PROC(errcode) BE_GetLastError ( short funcId, const ExprEnv& /* environment */, const DataVect& /* parameters */, Data& results)
{
	errcode error = kNoError; // do not use NoError();
	
	if ( funcId == kBE_GetLastError ) {
		SetResult ( g_last_error, results );
		g_last_error = kNoError;
	} else if ( funcId == kBE_GetLastDDLError ) {
		SetResult ( g_last_ddl_error, results );
		g_last_ddl_error = kNoError;
	}

	return MapError ( error );
	
}



#pragma mark -
#pragma mark Clipboard
#pragma mark -

FMX_PROC(errcode) BE_ClipboardFormats ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& /* parameters */, Data& results)
{
	return TextConstantFunction ( ClipboardFormats(), results );	
}


FMX_PROC(errcode) BE_ClipboardData ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		WStringAutoPtr atype = ParameterAsWideString ( parameters, 0 );
		StringAutoPtr clipboard_contents = ClipboardData ( atype );
		SetResult ( clipboard_contents, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_ClipboardData


FMX_PROC(errcode) BE_SetClipboardData ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {

		StringAutoPtr to_copy = ParameterAsUTF8String ( parameters, 0 );
		WStringAutoPtr atype = ParameterAsWideString ( parameters, 1 );
		bool success = SetClipboardData ( to_copy, atype );
		SetResult ( success, results );

	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_SetClipboardData


#pragma mark -
#pragma mark Files & Folders
#pragma mark -


#pragma NOTE (consider refatoring some of the detail from the file functions into BEFileSystem)


FMX_PROC(errcode) BE_CreateFolder ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{
	errcode error = NoError();	
	
	try {

		WStringAutoPtr folder = ParameterAsWideString ( parameters, 0 );
		path directory_path = *folder;
		
		try {
			create_directory ( directory_path );
		} catch ( filesystem_error& e ) {
			g_last_error = e.code().value();
		}
		
		SetResult ( g_last_error, results );
				
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_CreateFolder


FMX_PROC(errcode) BE_DeleteFile ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		WStringAutoPtr file = ParameterAsWideString ( parameters, 0 );
		path path = *file;
		
		try {
			remove_all ( path ); // if path is a directory then path and all it's contents are deleted
		} catch ( filesystem_error& e ) {
			g_last_error = e.code().value();
		}
		
		SetResult ( g_last_error, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_DeleteFile


FMX_PROC(errcode) BE_FileExists ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		WStringAutoPtr file = ParameterAsWideString ( parameters, 0 );
		
		path path = *file;			
		bool file_exists = exists ( path );

		SetResult ( file_exists, results );
		
	} catch ( filesystem_error& e ) {
		g_last_error = e.code().value();
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_FileExists


FMX_PROC(errcode) BE_ReadTextFromFile ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {

		WStringAutoPtr file = ParameterAsWideString ( parameters, 0 );
		StringAutoPtr contents;
		
		try {
			contents = ReadFileAsUTF8 ( file );
		} catch ( filesystem_error& e ) {
			g_last_error = e.code().value();
		}
		
		SetResult ( contents, results );

	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_ReadTextFromFile


FMX_PROC(errcode) BE_WriteTextToFile ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
		
	try {
		
		WStringAutoPtr file = ParameterAsWideString ( parameters, 0 );
		path path = *file;
		
		// should the text be appended to the file or replace any existing contents
		
		ios_base::openmode mode = ios_base::trunc;
		if ( parameters.Size() == 3 ) {
			bool append = parameters.AtAsBoolean ( 2 );
			if ( append ) {
				mode = ios_base::app;
			}
		}
		
		StringAutoPtr text_to_write = ParameterAsUTF8String ( parameters, 1 );
		vector<char> out = ConvertTextTo ( (char *)text_to_write->c_str(), text_to_write->size(), g_text_encoding );
		
		try {
			
			boost::filesystem::path filesystem_path ( path );
			boost::filesystem::ofstream output_file ( filesystem_path, ios_base::out | mode );
			output_file.exceptions ( boost::filesystem::ofstream::badbit | boost::filesystem::ofstream::failbit );			
			
			output_file.write ( &out[0], out.size() );
			output_file.close();

		} catch ( filesystem_error& e ) {
			g_last_error = e.code().value();
		} catch ( exception& e ) {
			g_last_error = errno; // unable to write to the file
		}
		
		SetResult ( g_last_error, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_WriteTextToFile


/*
 filemaker can create DDRs that contains utf-16 characters that are not
 valid in an XML document. reads the DDR and writes out a new one, skipping
 any invalid characters, and replaces the old file
 */

FMX_PROC(errcode) BE_StripInvalidUTF16CharactersFromXMLFile ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		WStringAutoPtr file = ParameterAsWideString ( parameters, 0 );
		path source = *file;

		wstring output_path = *file + TEMPORARY_FILE_SUFFIX;
		path destination = output_path;
		boost::uintmax_t length = file_size ( source ); // throws if the source does not exist
		
		boost::filesystem::ifstream input_file ( source, ios_base::in | ios_base::binary | ios_base::ate );
		input_file.seekg ( 0, ios::beg );
		boost::filesystem::ofstream output_file ( destination, ios_base::out | ios_base::binary | ios_base::ate );

		const size_t size = 2; // read (and write) 2 bytes at a time
		boost::uintmax_t skipped = 0;
		bool big_endian = true;

		for ( boost::uintmax_t i = 0; i < length; i += size ) {

			char codepoint[size];
			input_file.read ( codepoint, size );
			
			// check the bom, if present, to determin if the utf-16 if big or little endian
			if ( (i == 0) && ((unsigned char)codepoint[0] == 0xff && (unsigned char)codepoint[1] == 0xfe ) ) {
				big_endian = false;
			}
			
			// swap the byte order for big-endian files
			unichar * utf16 = (unichar *)codepoint;
			char byte_swapped[size];
			if ( big_endian ) {
				byte_swapped[0] = codepoint[1];
				byte_swapped[1] = codepoint[0];
				utf16 = (unichar *)byte_swapped;
			}
			
			// only check codepoints in the bmp (so no 4-byte codepoints)
			if ( (*utf16 == 0x9) ||
				(*utf16 == 0xA) ||
				(*utf16 == 0xD) ||
				((*utf16 >= 0x20) && (*utf16 <= 0xD7FF)) ||
				((*utf16 >= 0xE000) && (*utf16 <= 0xFFFE)) ) {
				
				output_file.write ( codepoint, size );

			} else {
				skipped += size;
			}
		}

		output_file.close();
		input_file.close ();
		
		/*
		 only replace the file if that we've skipped some characters and
		 the output file is the right size
		 */
		
		if ( (skipped > 0) && (length == (file_size ( destination ) + skipped)) ) {
			remove_all ( source );
			rename ( destination, source );
		} else {
			remove_all ( destination );
			if ( skipped > 0 ) {
				// if characters were skipped and the file size is wrong report an error
				error = kFileSystemError;
			}
		}

		SetResult ( error == kNoError, results );
		
	} catch ( filesystem_error& e ) {
		g_last_error = e.code().value();
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_StripInvalidUTF16CharactersFromXMLFile


FMX_PROC(errcode) BE_MoveFile ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		WStringAutoPtr from = ParameterAsWideString ( parameters, 0 );
		WStringAutoPtr to = ParameterAsWideString ( parameters, 1 );

		try {
			
			path from_path = *from;
			path to_path = *to;
			
			rename ( from_path, to_path );			
		} catch ( filesystem_error& e ) {
			g_last_error = e.code().value();
		}
		
		SetResult ( g_last_error, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_MoveFile


FMX_PROC(errcode) BE_CopyFile ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		WStringAutoPtr from = ParameterAsWideString ( parameters, 0 );
		WStringAutoPtr to = ParameterAsWideString ( parameters, 1 );
		
		try {
			
			path from_path = *from;
			path to_path = *to;
			
			recursive_directory_copy ( from_path, to_path );
			
		} catch ( filesystem_error& e ) {
			g_last_error = e.code().value();
		}
		
		SetResult ( g_last_error, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_CopyFile



FMX_PROC(errcode) BE_ListFilesInFolder ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		TextAutoPtr list_of_files;
		TextAutoPtr end_of_line;
		end_of_line->Assign ( "\r" );

		WStringAutoPtr directory = ParameterAsWideString ( parameters, 0 );

		try {

			path directory_path = *directory;
			
			if ( exists ( directory_path ) ) {
				
				directory_iterator end_itr; // default construction yields past-the-end
				directory_iterator itr ( directory_path );

				long want = ParameterAsLong ( parameters, 1, kBE_FileType_File );
				
				while ( itr != end_itr ) {
					
					bool is_folder = is_directory ( itr->status() );
				
					if ( 
							(!is_folder && (want == kBE_FileType_File)) ||
							(is_folder && (want == kBE_FileType_Folder)) ||
							(want == kBE_FileType_ALL)
						) {
						TextAutoPtr file_name;
						file_name->AssignWide ( itr->path().filename().wstring().c_str() );
						list_of_files->AppendText ( *file_name );
						list_of_files->AppendText ( *end_of_line );
					}
					
					++itr;
					
				}

			}

		} catch ( filesystem_error& e ) {
			g_last_error = e.code().value();
		}

		SetResult ( *list_of_files, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_ListFilesInFolder



#pragma mark -
#pragma mark Dialogs
#pragma mark -

FMX_PROC(errcode) BE_SelectFile ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {

		WStringAutoPtr prompt = ParameterAsWideString ( parameters, 0 );
		WStringAutoPtr inFolder = ParameterAsWideString ( parameters, 1 );
		WStringAutoPtr file = SelectFile ( prompt, inFolder );
		SetResult ( file, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_SelectFile


FMX_PROC(errcode) BE_SelectFolder ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {

		WStringAutoPtr prompt = ParameterAsWideString ( parameters, 0 );
		WStringAutoPtr inFolder = ParameterAsWideString ( parameters, 1 );
		WStringAutoPtr folder = SelectFolder ( prompt, inFolder );
		SetResult ( folder, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}

	return MapError ( error );
	
} // BE_SelectFolder


FMX_PROC(errcode) BE_DisplayDialog ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
		
	try {

		WStringAutoPtr title = ParameterAsWideString ( parameters, 0 );
		WStringAutoPtr message = ParameterAsWideString ( parameters, 1 );
		WStringAutoPtr ok_button = ParameterAsWideString ( parameters, 2 );
		WStringAutoPtr cancel_button = ParameterAsWideString ( parameters, 3 );
		WStringAutoPtr alternate_button = ParameterAsWideString ( parameters, 4 );
	
		int response = DisplayDialog ( title, message, ok_button, cancel_button, alternate_button );
		SetResult ( response, results );

	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}

	return MapError ( error );
	
} // BE_DisplayDialog


#pragma mark -
#pragma mark XSLT
#pragma mark -

FMX_PROC(errcode) BE_ApplyXSLT ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {

		StringAutoPtr xml_path = ParameterAsUTF8String ( parameters, 0 );
		StringAutoPtr xslt = ParameterAsUTF8String ( parameters, 1 );
		StringAutoPtr csv_path = ParameterAsUTF8String ( parameters, 2 );

		results.SetAsText( *ApplyXSLT ( xml_path, xslt, csv_path ), parameters.At(0).GetLocale() );
	
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}

	return MapError ( error );
	
} // BE_ApplyXSLT


FMX_PROC(errcode) BE_ApplyXSLTInMemory ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		StringAutoPtr xml = ParameterAsUTF8String ( parameters, 0 );
		StringAutoPtr xslt = ParameterAsUTF8String ( parameters, 1 );
		
		results.SetAsText( *ApplyXSLTInMemory ( xml, xslt ), parameters.At(0).GetLocale() );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_ApplyXSLTInMemory


FMX_PROC(errcode) BE_XPath ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{
	errcode error = NoError();
	
	try {
		
		StringAutoPtr xml = ParameterAsUTF8String ( parameters, 0 );
		StringAutoPtr xpath = ParameterAsUTF8String ( parameters, 1 );
		StringAutoPtr nsList ( new string );

		const unsigned long number_of_parameters = parameters.Size();
		if ( number_of_parameters > 2 ) {
			nsList = ParameterAsUTF8String ( parameters, 2 );
		}
		
		results.SetAsText( *ApplyXPath ( xml, xpath, nsList ), parameters.At(0).GetLocale() );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_XPath


FMX_PROC(errcode) BE_XPathAll ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{
	errcode error = NoError();
	TextAutoPtr text;
	
	try {

		StringAutoPtr xml = ParameterAsUTF8String ( parameters, 0 );
		StringAutoPtr xpath = ParameterAsUTF8String ( parameters, 1 );

		const unsigned long number_of_parameters = parameters.Size();
		StringAutoPtr nsList ( new string );
		if ( number_of_parameters > 2 ) {
			nsList = ParameterAsUTF8String ( parameters, 2 );
		}
				
		results.SetAsText(*ApplyXPathAll (xml, xpath, nsList), parameters.At(0).GetLocale() );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_XPathAll



#pragma mark -
#pragma mark User Preferences
#pragma mark -


FMX_PROC(errcode) BE_SetPreference ( short /*funcId*/, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		WStringAutoPtr key = ParameterAsWideString ( parameters, 0 );
		WStringAutoPtr value = ParameterAsWideString ( parameters, 1 );
		WStringAutoPtr domain = ParameterAsWideString ( parameters, 2 );
		
		if ( domain->empty() ) {
			domain->assign ( USER_PREFERENCES_DOMAIN );
		}

		SetResult ( SetPreference ( key, value, domain ), results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_SetPreference



FMX_PROC(errcode) BE_GetPreference ( short /*funcId*/, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		WStringAutoPtr key = ParameterAsWideString ( parameters, 0 );
		WStringAutoPtr domain = ParameterAsWideString ( parameters, 1 );
		
		if ( domain->empty() ) {
			domain->assign ( USER_PREFERENCES_DOMAIN );
		}
		
		SetResult ( GetPreference ( key, domain ), results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_GetPreference



#pragma mark -
#pragma mark Compression / Encoding
#pragma mark -



FMX_PROC(errcode) BE_Unzip ( short /*funcId*/, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{
	errcode error = NoError();
	
	try {
		
		StringAutoPtr archive = ParameterAsUTF8String ( parameters, 0 );
		
		SetResult ( UnZip ( archive ), results );
		
	} catch ( filesystem_error& e ) {
		g_last_error = e.code().value();
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_Unzip



FMX_PROC(errcode) BE_Zip ( short /*funcId*/, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{
	errcode error = NoError();
	
	try {
		
		StringAutoPtr file = ParameterAsUTF8String ( parameters, 0 );
		
		SetResult ( Zip ( file ), results );
		
	} catch ( filesystem_error& e ) {
		g_last_error = e.code().value();
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_Zip



FMX_PROC(errcode) BE_Base64_Decode ( short /*funcId*/, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{
	errcode error = NoError();
	
	try {
		
		StringAutoPtr text = ParameterAsUTF8String ( parameters, 0 );
		StringAutoPtr filename = ParameterAsUTF8String ( parameters, 1 );
		
		// throws if we do not lop off any padding
		boost::algorithm::trim_if ( *text, boost::algorithm::is_any_of ( " \t\f\v\n\r" ) );
//		unsigned long before = text->size();
		boost::algorithm::trim_right_if ( *text, boost::algorithm::is_any_of ( L"=" ) );
//		unsigned long skip = before > text->size();

		// decode it
//		vector<char> data ( base64_binary(text->begin()), base64_binary(text->end() - skip) );
		
		try {
			vector<char> data ( base64_binary ( text->begin() ), base64_binary ( text->end() ) );
			SetResult ( *filename, data, results );
		} catch ( dataflow_exception& e ) { // invalid_base64_character
			vector<char> data ( base64_binary ( text->begin() ), base64_binary ( text->end() - 1 ) );
			SetResult ( *filename, data, results );
		}
		
				
	} catch ( dataflow_exception& e ) { // invalid_base64_character
		g_last_error = e.code;
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_Base64_Decode



FMX_PROC(errcode) BE_Base64_Encode ( short /*funcId*/, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{
	errcode error = NoError();
	
	try {

		BinaryDataAutoPtr data ( parameters.AtAsBinaryData(0) );

		ulong size = 0;
		char * buffer = NULL;
		
		int count = data->GetCount();

		if ( count > 0 ) {

			QuadCharAutoPtr data_type ( 'F', 'I', 'L', 'E' ); 
			int which = data->GetIndex ( *data_type );
			if ( which != -1 ) {
				size = data->GetSize ( which );
				buffer = new char [ size ];
				data->GetData ( which, 0, size, (void *)buffer );
			} else {
				g_last_error = kRequestedDataIsMissingError;
			}

		} else {

			StringAutoPtr text = ParameterAsUTF8String ( parameters, 0 );
			size = text->size();
			buffer = new char [ size ];
			memcpy ( buffer, text->c_str(), size );
			
		}

		StringAutoPtr base64 ( new string ( base64_text(buffer), base64_text(buffer + size) ) );
		for ( ulong i = 0 ; i < (base64->length() % 4) ; i ++ ) {
			base64->append ( "=" );
		}
		SetResult ( base64, results );
		delete [] buffer;
				
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_Base64_Encode



FMX_PROC(errcode) BE_SetTextEncoding ( short /*funcId*/, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{
	errcode error = NoError();
	
	try {
		
		StringAutoPtr encoding = ParameterAsUTF8String ( parameters, 0 );
		SetTextEncoding ( *encoding );
		SetResult ( error, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_SetTextEncoding



#pragma mark -
#pragma mark HTTP / Curl
#pragma mark -


FMX_PROC(errcode) BE_GetURL ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{	
	errcode error = NoError();
	
	try {
		
		StringAutoPtr url = ParameterAsUTF8String ( parameters, 0 );
		StringAutoPtr filename = ParameterAsUTF8String ( parameters, 1 );
		StringAutoPtr username = ParameterAsUTF8String ( parameters, 2 );
		StringAutoPtr password = ParameterAsUTF8String ( parameters, 3 );
		
		// not saving to file so do not supply the filename here
		vector<char> data = GetURL ( *url, "", *username, *password );
		
		SetResult ( *filename, data, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_GetURL



FMX_PROC(errcode) BE_SaveURLToFile ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& /* results */ )
{	
	errcode error = NoError();
	
	try {
		
		StringAutoPtr url = ParameterAsUTF8String ( parameters, 0 );
		StringAutoPtr filename = ParameterAsUTF8String ( parameters, 1 );
		StringAutoPtr username = ParameterAsUTF8String ( parameters, 2 );
		StringAutoPtr password = ParameterAsUTF8String ( parameters, 3 );
		
		vector<char> data = GetURL ( *url, *filename, *username, *password );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_SaveURLToFile



FMX_PROC(errcode) BE_HTTP_POST ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{	
	errcode error = NoError();
	
	try {
		
		StringAutoPtr url = ParameterAsUTF8String ( parameters, 0 );
		StringAutoPtr post_parameters = ParameterAsUTF8String ( parameters, 1 );
		
		vector<char> data = HTTP_POST ( url, post_parameters );
		data.push_back ( '\0' );
		StringAutoPtr data_string ( new string ( &data[0] ) );
		SetResult ( data_string, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_HTTP_POST



FMX_PROC(errcode) BE_HTTP_Response_Code ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& /* parameters */, Data& results )
{	
	errcode error = NoError();
	
	try {
		
		SetResult ( g_http_response_code, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_HTTP_Response_Code



FMX_PROC(errcode) BE_HTTP_Response_Headers ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& /* parameters */, Data& results )
{	
	errcode error = NoError();
	
	try {
		
		StringAutoPtr headers ( new string ( g_http_response_headers ) );
		SetResult ( headers, results );

	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_HTTP_Response_Headers



FMX_PROC(errcode) BE_HTTP_Set_Custom_Header ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{	
	errcode error = NoError();
	
	try {
		
		StringAutoPtr name = ParameterAsUTF8String ( parameters, 0 );
		StringAutoPtr value = ParameterAsUTF8String ( parameters, 1 );
		
		if ( value->empty() ) {
			g_http_custom_headers.erase ( *name );
		} else {
			g_http_custom_headers [ *name ] = *value;
		}
		
		SetResult ( g_last_error, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_HTTP_Set_Custom_Header



#pragma mark -
#pragma mark Other / Ungrouped
#pragma mark -


/* 
 
 invoked for multiple plug-in functions... funcId is used to determine which one
 
 constants should be defined in BEPluginGlobalDefines.h
 
 each set of constants should have it's own range [ 1000 then 2000 then 3000 etc. ]
 with an offset of x000
 
*/

FMX_PROC(errcode) BE_NumericConstants ( short funcId, const ExprEnv& /* environment */, const DataVect& /* parameters */, Data& results)
{
	errcode error = NoError();
	
	try {
		
		SetResult ( funcId % kBE_NumericConstantOffset, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_NumericConstants


/*
 BE_ExtractScriptVariables implements are somewhat imperfect heuristic for finding
 script variables within chunks of filemaker calculation
 
 try to stip out unwanted text such as strings and comments and then, when a $ is
 found, attempt to guess the where the variable name ends
 */

FMX_PROC(errcode) BE_ExtractScriptVariables ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		BEWStringVector variables;
		WStringAutoPtr calculation = ParameterAsWideString ( parameters, 0 );

		wstring search_for = L"$/\""; // variables, comments and strings (including escaped strings)
		size_t found = calculation->find_first_of ( search_for );

		while ( found != wstring::npos )
		{
			size_t end = 0;
			size_t search_from = found + 1;
									
			switch ( calculation->at ( found ) ) {
				case L'$': // variables
				{
					/*
					 find the end of the variable
						+ - * / ^ & = ≠ < > ≤ ≥ ( , ; ) [ ] " :: $ }
					 unicode escapes are required on Windows
					 */
					
					end = calculation->find_first_of ( L" ;+-=*/&^<>\t\r[]()\u2260\u2264\u2265,", search_from );
					if ( end == wstring::npos ) {
						end = calculation->size();
					}

					// add the variable to the list
					wstring wanted = calculation->substr ( found, end - found );
					variables.PushBack ( wanted );
					search_from = end + 1;
				}
				break;
					
				case L'/': // comments
					switch ( calculation->at ( search_from ) ) {
						case L'/':
							end = calculation->find ( L"\r", search_from );
							search_from = end + 1;
							break;
							
						case L'*':
							end = calculation->find ( L"*/", search_from );
							search_from = end + 2;
							break;
							
						default:
							break;
					}
					break;
					
				case L'\"': // escaped strings
					end = calculation->find ( L"\"", search_from );
					while ( (end != string::npos) && (calculation->at ( end - 1 ) == L'\\') ) {
						end = calculation->find ( L"\"", end + 1 );
					}
					search_from = end + 1;
					break;
					
//				default:
			}
			
			// this is not on an eternal quest
			if ( (end != string::npos) && (search_from < calculation->size()) ) { 
				found = calculation->find_first_of ( search_for, search_from );
			} else {
				found = string::npos;
			}
		}
		
		results.SetAsText( *(variables.AsValueList()), parameters.At(0).GetLocale() );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_ExtractScriptVariables



FMX_PROC(errcode) BE_ExecuteShellCommand ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		StringAutoPtr command = ParameterAsUTF8String ( parameters, 0 );
		bool waitForResponse = ParameterAsBoolean ( parameters, 1 );

		StringAutoPtr response ( new string );

		if ( waitForResponse ) {
			g_last_error = ExecuteShellCommand ( *command, *response );
		} else {
			boost::thread dontWaitForThis ( ExecuteShellCommand, *command, *response );
		}

		SetResult ( response, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_ExecuteShellCommand


/*
 a wrapper for the FileMaker SQL calls FileMaker_Tables and FileMaker_Fields
 
 under FileMaker 11 the functions return
 
 FileMaker_Tables - returns a list of TOs with associated information
	table occurance name
	table occurance id
	table name
	file name
	schema modification count 

 FileMaker_Fields - returns a list of fields...
	table occurance name
	name
	type
	id
	class
	repitions
	modification count 

 Note: 
	For FileMaker versions 8.5~10 a subset of this information is returned. 
	The functions do not exist in versions 7 & 8 and an error is returned.
 
 */

FMX_PROC(errcode) BE_FileMaker_TablesOrFields ( short funcId, const ExprEnv& environment, const DataVect& /* parameters */, Data& reply )
{	
	errcode error = NoError();
	
	TextAutoPtr expression;

	try {

		if ( funcId == kBE_FileMaker_Tables ) {
			expression->Assign ( "SELECT * FROM FileMaker_Tables" );
		} else {
			expression->Assign ( "SELECT * FROM FileMaker_Fields" );
		}
		
		// the original api best suits the purpose
		error = environment.ExecuteSQL ( *expression, reply, '\t', '\n' );
		
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}	
	
	return MapError ( error );
	
} // BE_FileMaker_TablesOrFields


// open the supplied url in the user's default browser

FMX_PROC(errcode) BE_OpenURL ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& reply )
{	
	errcode error = NoError();
	
	TextAutoPtr expression;
	
	try {
		
		WStringAutoPtr url = ParameterAsWideString ( parameters, 0 );
		bool succeeded = OpenURL ( url );

		SetResult ( succeeded, reply );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}	
	
	return MapError ( error );
	
} // BE_OpenURL



FMX_PROC(errcode) BE_OpenFile ( short /*funcId*/, const ExprEnv& /* environment */, const DataVect& parameters, Data& results)
{
	errcode error = NoError();
	
	try {
		
		WStringAutoPtr path = ParameterAsWideString ( parameters, 0 );
		
		bool succeeded = OpenFile ( path );
		SetResult ( succeeded, results );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}
	
	return MapError ( error );
	
} // BE_OpenFile



FMX_PROC(errcode) BE_ExecuteScript ( short /* funcId */, const ExprEnv& environment, const DataVect& parameters, Data& reply)
{
	errcode error = 0;
	
	try {
		
		TextAutoPtr script_name;
		script_name->SetText ( parameters.AtAsText ( 0 ) );

		
		// use the current file when a file name is not provided
		
		TextAutoPtr file_name;
		DataAutoPtr parameter;

		ulong number_of_paramters = parameters.Size();
		
		if ( number_of_paramters >= 2 ) {
			file_name->SetText ( parameters.AtAsText ( 1 ) );
		} else {
			TextAutoPtr command;
			command->Assign ( "Get ( FileName )" );

			DataAutoPtr name;
			environment.Evaluate ( *command, *name );
			file_name->SetText ( name->GetAsText() );
		}

		// get the parameter, if present
		
		if ( number_of_paramters == 3 ) {
			LocaleAutoPtr default_locale;
			parameter->SetAsText ( parameters.AtAsText ( 2 ), *default_locale );
		}
		
		error = FMX_StartScript ( &(*file_name), &(*script_name), kFMXT_Pause, &(*parameter) );
		
		SetResult ( error, reply );

	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}	
	
	return MapError ( error );
	
} // BE_ExecuteScript



FMX_PROC(errcode) BE_FileMakerSQL ( short /* funcId */, const ExprEnv& environment, const DataVect& parameters, Data& results )
{	
	errcode error = NoError();
	
	try {
		
		TextAutoPtr expression;
		expression->SetText ( parameters.AtAsText(0) );
		
		ulong number_of_paramters = parameters.Size();

		TextAutoPtr filename;
		if ( number_of_paramters == 4 ) {
			filename->SetText ( parameters.AtAsText(3) );
		}
		
		
		BESQLCommand * sql = new BESQLCommand ( expression, filename );
		
		
		if ( number_of_paramters >= 2 ) {
			TextAutoPtr column_separator;
			column_separator->SetText ( parameters.AtAsText(1) );
			sql->set_column_separator ( column_separator );
		}
		
		if ( number_of_paramters >= 3 ) {
			TextAutoPtr row_separator;
			row_separator->SetText ( parameters.AtAsText(2) );
			sql->set_row_separator ( row_separator );
		}
		
		sql->execute ( environment );

		results.SetAsText( *(sql->get_text_result()), parameters.At(0).GetLocale() );
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}	
	
	return MapError ( error );
	
} // BE_FileMakerSQL



FMX_PROC(errcode) BE_MessageDigest ( short /* funcId */, const ExprEnv& /* environment */, const DataVect& parameters, Data& results )
{	
	errcode error = NoError();
		
	try {
		
		StringAutoPtr message = ParameterAsUTF8String ( parameters, 0 );
		unsigned long type = ParameterAsLong( parameters, 1, kBE_MessageDigestType_SHA256 );

		StringAutoPtr digest;

		if ( type == kBE_MessageDigestType_MD5 ) {
			digest = MD5 ( message );
		} else { // the default is SHA256
			digest = SHA256 ( message );
		}
		
		SetResult ( digest, results );
		
		
	} catch ( bad_alloc& e ) {
		error = kLowMemoryError;
	} catch ( exception& e ) {
		error = kErrorUnknown;
	}	
	
	return MapError ( error );
	
} // BE_FileMaker_TablesOrFields


