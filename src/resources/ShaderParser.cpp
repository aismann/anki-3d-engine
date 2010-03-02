#include <iomanip>
#include "ShaderParser.h"
#include "Scanner.h"
#include "parser.h"
#include "Util.h"


//=====================================================================================================================================
// printSourceLines                                                                                                                   =
//=====================================================================================================================================
void ShaderParser::printSourceLines() const
{
	for( uint i=0; i<sourceLines.size(); ++i )
	{
		PRINT( setw(3) << i+1 << ": " << sourceLines[i] );
	}
}


//=====================================================================================================================================
// printShaderVars                                                                                                                    =
//=====================================================================================================================================
void ShaderParser::printShaderVars() const
{
	PRINT( "TYPE" << setw(20) << "NAME" << setw(4) << "LOC" );
	for( uint i=0; i<output.attributes.size(); ++i )
	{
		PRINT( setw(4) << "A" << setw(20) << output.attributes[i].name << setw(4) << output.attributes[i].customLoc );
	}
}


//=====================================================================================================================================
// findShaderVar                                                                                                                      =
//=====================================================================================================================================
Vec<ShaderParser::ShaderVarPragma>::iterator ShaderParser::findShaderVar( Vec<ShaderVarPragma>& vec, const string& name ) const
{
	Vec<ShaderVarPragma>::iterator it = vec.begin();
	while( it != vec.end() && it->name != name )
	{
		++it;
	}
	return it;
}


//=====================================================================================================================================
// parseFileForPragmas                                                                                                                =
//=====================================================================================================================================
bool ShaderParser::parseFileForPragmas( const string& filename, int depth )
{
	// first check the depth
	if( depth > 99 )
	{
		ERROR( "The include depth is too high. Probably circular includance (Im in file \"" << filename << "\")" );
		return false;
	}

	// load file in lines
	Vec<string> lines = Util::getFileLines( filename.c_str() );
	if( lines.size() < 1 )
	{
		ERROR( "Cannot parse file \"" << filename << "\"" );
		return false;
	}

	// scanner
	Scanner scanner( false );
	if( !scanner.loadFile( filename.c_str() ) ) return false;
	const Scanner::Token* token;

	
	do
	{
		token = &scanner.getNextToken();

		if( token->code == Scanner::TC_SHARP )
		{
			token = &scanner.getNextToken();
			if( token->code == Scanner::TC_IDENTIFIER && strcmp(token->value.string, "pragma") == 0 )
			{
				token = &scanner.getNextToken();
				if( token->code == Scanner::TC_IDENTIFIER && strcmp(token->value.string, "anki") == 0 )
				{
					token = &scanner.getNextToken();
/* vertShaderBegins */
					if( token->code == Scanner::TC_IDENTIFIER && strcmp(token->value.string, "vertShaderBegins") == 0 )
					{
						// play

						// its defined in same place so there is probable circular includance
						if( vertShaderBegins.definedInLine==scanner.getLineNmbr() && vertShaderBegins.definedInFile==filename )
						{
							PARSE_ERR( "vertShaderBegins already defined in the same place. Check for circular or multiple includance" );
							return false;
						}

						if( vertShaderBegins.definedInLine != -1 ) // already defined elsewhere so throw error
						{
							PARSE_ERR( "vertShaderBegins already defined at " << vertShaderBegins.definedInFile << ":" <<
							           vertShaderBegins.definedInLine );
							return false;
						}

						if( fragShaderBegins.definedInLine != -1 ) // frag shader should be after vert
						{
							PARSE_ERR( "vertShaderBegins must precede fragShaderBegins defined at " << fragShaderBegins.definedInFile <<
							           ":" << fragShaderBegins.definedInLine );
							return false;
						}
						
						vertShaderBegins.definedInFile = filename;
						vertShaderBegins.definedInLine = scanner.getLineNmbr();
						vertShaderBegins.globalLine = sourceLines.size() + 1;
						sourceLines.push_back( string("#line ") + Util::intToStr(scanner.getLineNmbr()) + ' ' + Util::intToStr(depth) + " // " + lines[scanner.getLineNmbr()-1] );
						// stop play
					}
/* fragShaderBegins */
					else if( token->code == Scanner::TC_IDENTIFIER && strcmp(token->value.string, "fragShaderBegins") == 0 )
					{
						// play

						// its defined in same place so there is probable circular includance
						if( fragShaderBegins.definedInLine==scanner.getLineNmbr() && fragShaderBegins.definedInFile==filename )
						{
							PARSE_ERR( "fragShaderBegins already defined in the same place. Check for circular or multiple includance" );
							return false;
						}

						if( fragShaderBegins.definedInLine != -1 ) // if already defined elsewhere throw error
						{
							PARSE_ERR( "fragShaderBegins already defined at " << fragShaderBegins.definedInFile << ":" <<
							           fragShaderBegins.definedInLine );
							return false;
						}
						
						if( vertShaderBegins.definedInLine == -1 ) // vert shader entry point not defined
						{
							PARSE_ERR( "fragShaderBegins should be defined after vertShaderBegins" );
							return false;
						}

						fragShaderBegins.definedInFile = filename;
						fragShaderBegins.definedInLine = scanner.getLineNmbr();
						fragShaderBegins.globalLine = sourceLines.size() + 1;
						sourceLines.push_back( string("#line ") + Util::intToStr(scanner.getLineNmbr()) + ' ' + Util::intToStr(depth) + " // " + lines[scanner.getLineNmbr()-1] );
						// stop play
					}
/* include */
					else if( token->code == Scanner::TC_IDENTIFIER && strcmp(token->value.string, "include") == 0 )
					{
						token = &scanner.getNextToken();
						if( token->code == Scanner::TC_STRING )
						{
							// play
							//int line = sourceLines.size();
							sourceLines.push_back( string("#line 0 ") + Util::intToStr(depth+1) + " // " + lines[scanner.getLineNmbr()-1] );
							if( !parseFileForPragmas( token->value.string, depth+1 ) ) return false;
							sourceLines.push_back( string("#line ") + Util::intToStr(scanner.getLineNmbr()) + ' ' + Util::intToStr(depth) +  " // end of " + lines[scanner.getLineNmbr()-1] );
							// stop play
						}
						else
						{
							PARSE_ERR_EXPECTED( "string" );
							return false;
						}
					}
/* attribute */
					else if( token->code == Scanner::TC_IDENTIFIER && strcmp(token->value.string, "attribute") == 0 )
					{
						token = &scanner.getNextToken();
						if( token->code == Scanner::TC_IDENTIFIER )
						{
							string varName = token->value.string;
							token = &scanner.getNextToken();
							if( token->code == Scanner::TC_NUMBER && token->type == Scanner::DT_INT )
							{
								int loc = token->value.int_;

								// check if already defined and for circular includance
								Vec<ShaderVarPragma>::iterator attrib = findShaderVar( output.attributes, varName );
								if( attrib != output.attributes.end() )
								{
									if( attrib->definedInLine==scanner.getLineNmbr() && attrib->definedInFile==filename )
									{
										PARSE_ERR( "\"" << varName << "\" already defined in the same place. Check for circular or multiple includance" );
									}
									else
									{
										PARSE_ERR( "Attribute \"" << varName << "\" already defined at " << attrib->definedInFile << ":" << attrib->definedInLine );
									}
									return false;
								}
								// search if another var has the same loc
								for( attrib = output.attributes.begin(); attrib!=output.attributes.end(); ++attrib )
								{
									if( attrib->customLoc == loc )
									{
										PARSE_ERR( "The attributes \"" << attrib->name << "\" (" << attrib->definedInFile << ":" << attrib->definedInLine <<
										           ") and \"" << varName << "\" share the same location" );
										return false;
									}
								}
								
								// all ok, push it back
								output.attributes.push_back( ShaderVarPragma( filename, scanner.getLineNmbr(), varName, loc ) );
								sourceLines.push_back( lines[scanner.getLineNmbr()-1] );
							}
							else
							{
								PARSE_ERR_EXPECTED( "integer" );
								return false;
							}
						}
						else
						{
							PARSE_ERR_EXPECTED( "identifier" );
							return false;
						}
					}
					else
					{
						PARSE_WARN( "Ignoring incorrect #pragma anki" );
					}
				} // end if anki
				
				token = &scanner.getNextToken();
				if( token->code!=Scanner::TC_NEWLINE && token->code!=Scanner::TC_EOF )
				{
					PARSE_ERR_EXPECTED( "newline or end of file" );
					return false;
				}
				
				if( token->code == Scanner::TC_EOF )
					break;
				
			} // end if pragma
		} // end if #
/* newline */		
		else if( token->code == Scanner::TC_NEWLINE )
		{
			sourceLines.push_back( lines[ scanner.getLineNmbr() - 2 ] );
			//PRINT( lines[ scanner.getLineNmbr() - 2 ] )
		}
/* EOF */
		else if( token->code == Scanner::TC_EOF )
		{
			sourceLines.push_back( lines[ scanner.getLineNmbr() - 1 ] );
			//PRINT( lines[ scanner.getLineNmbr() - 1 ] )
			break;
		}
/* error */
		else if( token->code == Scanner::TC_ERROR )
		{
			return false;
		}

	} while( true );
	
	return true;
}


//=====================================================================================================================================
// parseFile                                                                                                                          =
//=====================================================================================================================================
bool ShaderParser::parseFile( const char* filename )
{
	// parse master file
	if( !parseFileForPragmas( filename ) ) return false;

	// sanity checks
	if( vertShaderBegins.globalLine == -1 )
	{
		ERROR( "Entry point \"vertShaderBegins\" is not defined in file \"" << filename << "\"" );
		return false;
	}
	if( fragShaderBegins.globalLine == -1 )
	{
		ERROR( "Entry point \"fragShaderBegins\" is not defined in file \"" << filename << "\"" );
		return false;
	}
	
	// construct shader's source code
	output.vertShaderSource = "";
	output.fragShaderSource = "";
	for( int i=0; i<vertShaderBegins.globalLine-1; ++i )
	{
		output.vertShaderSource += sourceLines[i] + "\n";
		output.fragShaderSource += sourceLines[i] + "\n";
	}	
	for( int i=vertShaderBegins.globalLine-1; i<fragShaderBegins.globalLine-1; ++i )
	{
		output.vertShaderSource += sourceLines[i] + "\n";
	}
	for( int i=fragShaderBegins.globalLine-1; i<int(sourceLines.size()); ++i )
	{
		output.fragShaderSource += sourceLines[i] + "\n";
	}
	
	//PRINT( "vertShaderBegins.globalLine: " << vertShaderBegins.globalLine )
	//PRINT( "fragShaderBegins.globalLine: " << fragShaderBegins.globalLine )
	//printSourceLines();
	//printShaderVars();

	return true;
}

