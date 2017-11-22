#include "stdafx.h"
#include "Common.h"
#include "Armor.h"
#include "Decoration.h"
#include "CharmDatabase.h"
#include "LoadedData.h"

using namespace System;
using namespace Windows::Forms;

#using <mscorlib.dll>
using namespace System::Runtime::InteropServices;

[DllImport("user32.dll", SetLastError = true)]
extern int SendMessage( IntPtr hWnd, int Msg, bool wParam, Int32 lParam );

//[DllImport("user32.dll", SetLastError = true)]
//extern IntPtr PostMessage( IntPtr hWnd, int Msg, IntPtr wParam, IntPtr lParam );

//[DllImport("Kernel32.dll")]
//unsigned GetLastError();*/

namespace Utility
{
	void AddAbilitiesToMap( List_t< AbilityPair^ >% _list, Map_t< Ability^, int >% _map, const int mult )
	{
		for each( AbilityPair^ ap in _list )
		{
			if( _map.ContainsKey( ap->ability ) )
				_map[ ap->ability ] += ap->amount * mult;
			else _map.Add( ap->ability, ap->amount * mult );
		}
	}

	String^ RemoveQuotes( String^ in_out )
	{
		if( in_out == L"" ) return in_out;
		in_out = in_out->Trim();
		if( in_out[ 0 ] == L'\"' ) in_out = in_out->Substring( 1 );
		if( in_out[ in_out->Length - 1 ] == L'\"' ) in_out = in_out->Substring( 0, in_out->Length - 1 );
		return in_out;
	}

	void SplitString( List_t< String^ >^ vec, String^ str, const wchar_t c )
	{
		str = str->Trim();
		int last_non_delim = 0;
		for( int i = 0; i < str->Length; ++i )
		{
			if( str[ i ] == c )
			{
				String^ substr = str->Substring( last_non_delim, i - last_non_delim );
				RemoveQuotes( substr );
				vec->Add( substr );
				last_non_delim = i + 1;
			}
		}
		str = str->Substring( last_non_delim, str->Length - last_non_delim );
		RemoveQuotes( str );
		vec->Add( str );
	}

	bool ContainsString( List_t< String^ >% vec, String^ item )
	{
		for( int i = 0; i < vec.Count; ++i )
			if( vec[ i ] == item ) return true;
		return false;
	}

	void FindLevelReqs( unsigned% available, unsigned% required, String^ input )
	{
		if( input->Length > 0 && input[ 0 ] == L'\"' ) input = Utility::RemoveQuotes( input );
		const int exclamation_point = input->LastIndexOf( L'!' );
		if( exclamation_point == -1 )
		{
			required = 0;
			available = Convert::ToUInt32( input );
			return;
		}
		required = Convert::ToUInt32( input->Substring( 0, exclamation_point ) );
		if( exclamation_point < input->Length - 1 )
			available = Convert::ToUInt32( input->Substring( exclamation_point + 1 ) );
	}

	String^ SlotString( const unsigned slots )
	{
		return slots == 0 ? L"---" :
			   slots == 1 ? L"O--" :
			   slots == 2 ? L"OO-" : L"OOO";
	}

	unsigned CountChars( String^ str, const wchar_t c )
	{
		unsigned total = 0;
		for( int i = 0; i < str->Length; ++i )
			if( str[ i ] == c ) ++total;
		return total;
	}

	void CopyTextToClipBoard( String^ data )
	{
		Clipboard::SetText( data );
	}

	void ContextMenuClicked( Object^ obj, EventArgs^ args )
	{
		ToolStripMenuItem^ item = safe_cast< ToolStripMenuItem^ >( obj );
		const int space = item->Text->IndexOf( L' ' );
		String^ material = item->Text->Substring( space + 1 );
		CopyTextToClipBoard( material );
	}

	void ContextMenuClicked2( System::Object^ obj, EventArgs^ args )
	{
		ToolStripMenuItem^ item = safe_cast< ToolStripMenuItem^ >( obj );
		CopyTextToClipBoard( item->Text );
	}

	void AddMenuItem( ToolStripItemCollection^ list, String^ name, const bool context2 )
	{
		ToolStripMenuItem^ item;
		if( context2 )
			item = gcnew ToolStripMenuItem( name, nullptr, gcnew EventHandler( ContextMenuClicked2 ) );
		else
			item = gcnew ToolStripMenuItem( name, nullptr, gcnew EventHandler( ContextMenuClicked ) );

		list->Add( item );
	}

	void GetAbilityMenu( Ability^ ability, ToolStripItemCollection^ strip )
	{
		AddMenuItem( strip, ability->name, true );
		strip->Add( L"-" );
		Ability::SkillMap_t::Enumerator e = ability->skills.GetEnumerator();
		while( e.MoveNext() )
		{
			AddMenuItem( strip, e.Current.Key + L": " + e.Current.Value->name, false );
			if( e.Current.Key < 0 )
				strip[ strip->Count - 1 ]->ForeColor = Drawing::Color::Red;
		}
	}

	void UpdateContextMenu( ContextMenuStrip^ strip, List_t< AbilityPair^ >% abilities )
	{
		for each( AbilityPair^ apair in abilities )
		{
			if( apair->amount != 0 && apair->ability != SpecificAbility::torso_inc )
			{
				AddMenuItem( strip->Items, Convert::ToString( apair->amount ) + L" " + apair->ability->name, false );
				ToolStripMenuItem^ mi = safe_cast< ToolStripMenuItem^ >( strip->Items[ strip->Items->Count - 1 ] );
				GetAbilityMenu( apair->ability, mi->DropDownItems );
			}
			else
				strip->Items->Add( apair->ability->name, nullptr, gcnew EventHandler( ContextMenuClicked2 ) );
		}
	}

	void UpdateContextMenu( ContextMenuStrip^ strip, Ability^ ability )
	{
		GetAbilityMenu( ability, strip->Items );
	}

	void UpdateContextMenu( ContextMenuStrip^ strip, List_t< MaterialComponent^ >^ components )
	{
		strip->Items->Add( L"-" );
		for each( MaterialComponent^ part in components )
		{
			AddMenuItem( strip->Items, Convert::ToString( part->amount ) + L"x " + part->material->name, false );
		}
	}

	void UpdateContextMenu( ContextMenuStrip^ strip, Decoration^ decoration )
	{
		if( StringTable::chinese )
		{
			if( decoration->eng_name )
				AddMenuItem( strip->Items, decoration->eng_name, true );

			AddMenuItem( strip->Items, decoration->chn_name, true );
		}
		else AddMenuItem( strip->Items, decoration->name, true );

		AddMenuItem( strip->Items, decoration->chn_name, true );
		strip->Items->Add( L"-" );
		strip->Items->Add( SlotString( decoration->slots_required ) );
		strip->Items->Add( L"-" );
		UpdateContextMenu( strip, decoration->abilities );
		UpdateContextMenu( strip, %decoration->components );
		if( decoration->components2.Count > 0 )
			UpdateContextMenu( strip, %decoration->components2 );
	}

	void UpdateContextMenu( ContextMenuStrip^ strip, Armor^ armor )
	{
		if( StringTable::chinese )
		{
			if( armor->eng_name )
				AddMenuItem( strip->Items, armor->eng_name, true );
		}
		else AddMenuItem( strip->Items, armor->name, true );

		AddMenuItem( strip->Items, armor->chn_name, true );
		
		strip->Items->Add( L"-" );
		strip->Items->Add( SlotString( armor->num_slots ) );
		strip->Items->Add( L"-" );
		strip->Items->Add( ColonString( Defence ) + FormatString2( To, armor->defence, armor->max_defence ) );
		strip->Items->Add( ColonString( FireRes ) + Convert::ToString( armor->fire_res ) );
		strip->Items->Add( ColonString( WaterRes ) + Convert::ToString( armor->water_res ) );
		strip->Items->Add( ColonString( ThunderRes ) + Convert::ToString( armor->thunder_res ) );
		strip->Items->Add( ColonString( IceRes ) + Convert::ToString( armor->ice_res ) );
		strip->Items->Add( ColonString( DragonRes ) + Convert::ToString( armor->dragon_res ) );
		strip->Items->Add( L"-" );
		UpdateContextMenu( strip, armor->abilities );
		UpdateContextMenu( strip, %armor->components );
	}

	ref class ToolHost : public ToolStripControlHost
	{
		RichTextBox rtb;
		String^ rtf;
	public:
		ToolHost( const unsigned table_number, const unsigned mystery, const unsigned shining, const unsigned timeworn )
			: ToolStripControlHost( %rtb )
		{
			using namespace System::Drawing;

			rtb.SelectionColor = Color::Black;
			rtb.AppendText( StartString( Table ) + Convert::ToString( table_number ) + L": " );
			rtb.SelectionColor = Color::Gray;
			rtb.AppendText( ConVal( mystery ) );
			
			rtb.SelectionColor = rtb.DefaultForeColor;
			rtb.AppendText( L", " );
			rtb.SelectionColor = Color::DarkGoldenrod;
			rtb.AppendText( ConVal( shining ) );

			rtb.SelectionColor = rtb.DefaultForeColor;
			rtb.AppendText( L", " );
			rtb.SelectionColor = Color::Red;
			rtb.AppendText( ConVal( timeworn ) );

			rtb.BorderStyle = System::Windows::Forms::BorderStyle::None;
			rtb.ReadOnly = true;
			rtf = rtb.Rtf;
		}

		String^ ConVal( const unsigned v )
		{
			if( v >= 64000 )
				return Convert::ToString( v ) + L"+";
			else return Convert::ToString( v );
		}

		virtual void OnPaint( PaintEventArgs^ args ) override
		{
			rtb.Rtf = rtf;
			ToolStripControlHost::OnPaint( args );
		}
	};

	CharmLocationDatum^ UpdateCharmLocationCache( String^ charm )
	{
		const unsigned num_slots = CountChars( charm->Substring( charm->Length - 3, 3 ), L'O' );
		unsigned points1 = 0;
		int points2 = 0;
		Ability^ ability1 = nullptr, ^ability2 = nullptr;
		if( charm[ 0 ] != L'O' )
		{
			Assert( num_slots >= 0 && num_slots < 4, L"Invalid number of slots" );
			const int comma = charm->IndexOf( L',' );
			if( comma == -1 )
			{
				const int space = charm->IndexOf( L' ' );
				Assert( space > 0, L"Bad charm string" );
				points1 = Convert::ToUInt32( charm->Substring( 1, space - 1 ) );
				ability1 = Ability::FindAbility( charm->Substring( space + 1, charm->Length - space - 5 ) );
			}
			else
			{
				const int space1 = charm->IndexOf( L' ' );
				Assert( space1 > 0, L"Bad charm string" );
				points1 = Convert::ToUInt32( charm->Substring( 1, space1 - 1 ) );
				String^ ab1_str = charm->Substring( space1 + 1, comma - space1 - 1 );
				ability1 = Ability::FindAbility( ab1_str );

				const int space2 = charm->IndexOf( L' ', comma + 2 );
				Assert( space2 > 0, L"Bad charm string" );
				String^ ab2_str = charm->Substring( comma + 2, space2 - comma - 2 );
				points2 = Convert::ToInt32( ab2_str );
				ability2 = Ability::FindAbility( charm->Substring( space2 + 1, charm->Length - space2 - 5 ) );
			}
		}
		Charm c( num_slots );
		if( ability1 )
			c.abilities.Add( gcnew AbilityPair( ability1, points1 ) );
		if( ability2 )
			c.abilities.Add( gcnew AbilityPair( ability2, points2 ) );

		CharmLocationDatum^ results = CharmDatabase::FindCharmLocations( %c );
		CharmDatabase::location_cache->Add( charm, results );
		return results;
	}

	void UpdateContextMenu( ContextMenuStrip^ strip, String^ charm, const unsigned table )
	{
		CharmLocationDatum^ results;
		if( CharmDatabase::location_cache->ContainsKey( charm ) )
			results = CharmDatabase::location_cache[ charm ];
		else
		{
			results = UpdateCharmLocationCache( charm );
		}
		
		if( results->example )
			strip->Items->Add( results->example );
		else if( table > 0 && table < 18 && results->examples[ table - 1 ] != nullptr )
			strip->Items->Add( results->examples[ table - 1 ] );
		else
			strip->Items->Add( charm );

		strip->Items->Add( L"-" );
		if( table > 0 && table < 18 )
		{
			if( results->table[ table - 1, 0 ] ||
				results->table[ table - 1, 1 ] ||
				results->table[ table - 1, 2 ] )
			{
				strip->Items->Add( gcnew ToolHost( table, results->table[ table - 1, 0 ], results->table[ table - 1, 1 ], results->table[ table - 1, 2 ] ) );
			}
			else strip->Items->Add( StaticString( UnknownCharm ) );
		}
		else
		{
			for( int i = 0; i < results->table->GetLength( 0 ); ++i )
			{
				if( results->table[ i, 0 ] ||
					results->table[ i, 1 ] ||
					results->table[ i, 2 ] )
				{
					strip->Items->Add( gcnew ToolHost( i + 1, results->table[ i, 0 ], results->table[ i, 1 ], results->table[ i, 2 ] ) );
				}
			}
		}
		strip->Invalidate();
	}

	bool Contains( List_t< Skill^ >^ cont, const Ability^ val )
	{
		for( int i = 0; i < cont->Count; ++i )
		{
			if( cont[ i ]->ability == val )
				return true;
		}
		return false;
	}

}

void Material::LoadMaterials( String^ filename )
{
#ifdef CREATE_MATERIALS
	return;
#endif

	static_materials.Clear();
	IO::StreamReader fin( filename );
	while( !fin.EndOfStream )
	{
		String^ line = fin.ReadLine();
		if( line == L"" ) continue;
		Material^ mat = gcnew Material;
		List_t< String^ > split;
		Utility::SplitString( %split, line, L',' );
		mat->name = split[ 0 ];
		//mat->ping_index = Convert::ToUInt32( split[ 0 ] );
		mat->difficulty = 0;
		for( int i = 1; i < split.Count; ++i )
		{
			if( split[i] == L"Arena" )
			{
				mat->arena = true;
			}
			else if( split[ i ] != L"" )
			{
				mat->difficulty = Convert::ToUInt32( split[ i ] );
			}
		}
		static_materials.Add( mat );
	}
	fin.Close();
	static_material_map.Clear();
	for each( Material^% mat in static_materials )
	{
		static_material_map.Add( mat->name, mat );
	}
}

Material^ Material::FindMaterial( String^ name )
{
	name = name->Trim();

	if( static_material_map.ContainsKey( name ) )
		return static_material_map[ name ];

#ifdef CREATE_MATERIALS
	Material^ mat = gcnew Material;
	mat->name = name;
	Material::static_materials.Add( mat );
	Material::static_material_map[ mat->name ] = mat;
	return mat;
#endif
	Clipboard::SetText( name );
	Assert( false, L"Material not found: " + name );
	return nullptr;
}

void Material::LoadLanguage( String^ filename )
{
	static_material_map.Clear();
	IO::StreamReader fin( filename );
	for( int i = 0; i < static_materials.Count; )
	{
		String^ line = fin.ReadLine();
		if( !line )
		{
			Windows::Forms::MessageBox::Show( L"Unexpected end of file: not enough lines of text in translated material list" );
			break;
		}
		if( line == L"" || line[ 0 ] == L'#' )
			continue;
		static_materials[ i ]->name = line;
		static_material_map.Add( line, static_materials[ i ] );
		i++;
	}
}

void StringTable::LoadLanguage( String^ lang )
{
	chinese = lang == "Traditional Chinese";
	english = lang == "English";

	String^ dir = L"Data/Languages/" + lang + L"/";
	//load strings
	{
		text = gcnew array< String^ >( (int)StringTable::StringIndex::NumStrings );
		IO::StreamReader fin( IO::File::OpenRead( dir + L"strings.txt" ) );
		int i;
		for( i = 0; i < text->Length; )
		{
			String^ line = fin.ReadLine();
			if( !line )
			{
				Windows::Forms::MessageBox::Show( L"Unexpected end of file: not enough lines of text in strings.txt" );
				break;
			}
			if( line == L"" || line[ 0 ] == L'#' )
				continue;
			text[ i++ ] = line;
		}
		fin.Close();
	}
	//load armor, skills, etc.
	SkillTag::LoadLanguage( dir + L"tags.txt" );
	Material::LoadLanguage( dir + L"components.txt" );

	Armor::LoadLanguage( dir + L"head.txt",  Armor::ArmorType::HEAD );
	Armor::LoadLanguage( dir + L"body.txt",  Armor::ArmorType::BODY );
	Armor::LoadLanguage( dir + L"arms.txt",  Armor::ArmorType::ARMS );
	Armor::LoadLanguage( dir + L"waist.txt", Armor::ArmorType::WAIST );
	Armor::LoadLanguage( dir + L"legs.txt",  Armor::ArmorType::LEGS );

	Decoration::LoadLanguage( dir + L"decorations.txt" );

	Skill::LoadLanguage( dir + L"skills.txt" );
	Skill::LoadDescriptions( dir + L"skill_descriptions.txt" );
	Skill::UpdateOrdering();
	Ability::UpdateOrdering();
}

String^ StripAmpersands( String^ input )
{
	String^ res = L"";
	for each( wchar_t c in input )
	{
		if( c != L'&' )
			res += c;
	}
	return res;
}

void myassert( const bool val, String^ message )
{
	if( !val )
		MessageBox::Show( message );
}

bool ConvertInt( int% i, String^ str, StringTable::StringIndex err )
{
	int temp;
	try
	{
		const int limit = ( str[ 0 ] == L'-' ) ? 3 : 2;
		if( str->Length > limit )
			temp = Convert::ToInt32( str->Substring( 0, limit ) );
		else temp = Convert::ToInt32( str );
	}
	catch( FormatException^ )
	{
		MessageBox::Show( StringTable::text[(int)err ], StaticString( Error ), MessageBoxButtons::OK, MessageBoxIcon::Error );
		return false;
	}
	i = temp;
	return true;
}

bool ConvertUInt( unsigned% i, String^ str, StringTable::StringIndex err )
{
	unsigned temp;
	try
	{
		temp = Convert::ToUInt32( str );
	}
	catch( FormatException^ )
	{
		MessageBox::Show( StringTable::text[(int)err ], StaticString( Error ), MessageBoxButtons::OK, MessageBoxIcon::Error );
		return false;
	}
	i = temp;
	return true;
}

void SuspendDrawing( System::Windows::Forms::Control^ control )
{
	SendMessage( control->Handle, 11, false, 0 );
}

void ResumeDrawing( System::Windows::Forms::Control^ control )
{
	SendMessage( control->Handle, 11, true, 0 );
	control->Refresh();
}
