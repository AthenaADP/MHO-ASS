#include "stdafx.h"
#include "CharmDatabase.h"
#include "Solution.h"
#include "Armor.h"
#include "Skill.h"
#include <fstream>
#include <cmath>

using namespace System;

unsigned CalcMaxCharmType( Query^ query )
{
	const int hr_to_charm_type[] = 
	{
		0, // HR0+ mystery
		0,
		0,
		1, // HR3+ shining
		1, 
		2, // HR5+ timeworn
		2,
		2,
		2, // HR8+ (special permit)
	};

	return hr_to_charm_type[ query->hr ];
}

String^ FixTypos( String^ input )
{
	if( input == L"Ammo Accountant" )
		return L"Ammo Depot";
	else if( input == L"Guard Up" )
		return L"Guard Boost";
	else if( input == L"Frenzy" )
		return L"Rage";
	else if( input == L"Quagmire" )
		return L"Predicament";
	else return input;
}

#pragma region Custom charms/gear

#define CUSTOM_CHARM_TXT L"Data/mycharms.txt"

void CharmDatabase::SaveCustom()
{
	//slots,skill1,num,skill2,num,skill3,num
	IO::StreamWriter fout( CUSTOM_CHARM_TXT );
	fout.WriteLine( L"#Format: Slots,Skill1,Points1,Skill2,Points2,Skill3,Points3" );
	for each( Charm^ ch in mycharms )
	{
		fout.Write( Convert::ToString( ch->num_slots ) );
		for( int i = 0; i < 3; ++i )
		{
			if( i < ch->abilities.Count )
				fout.Write( L"," + ch->abilities[ i ]->ability->name + L"," + Convert::ToString( ch->abilities[ i ]->amount ) );
			else fout.Write( L",," );
		}
		fout.WriteLine();
	}
}

bool CharmDatabase::CreateNewCustom()
{
	if( !IO::File::Exists( CUSTOM_CHARM_TXT ) )
	{
		mycharms.Clear();
		/*Charm^ charm = gcnew Charm( 0 );
		charm->abilities.Add( gcnew AbilityPair( SpecificAbility::gathering, 10 ) );
		charm->custom = true;
		mycharms.Add( charm );*/
		return true;
	}
	return false;
}

List_t< Charm^ >^ CharmDatabase::LoadCharms( System::String^ filename )
{
	List_t< Charm^ >^ results = gcnew List_t< Charm^ >();

	IO::StreamReader fin( filename );
	String^ temp;
	while( !fin.EndOfStream )
	{
		temp = fin.ReadLine();
		if( temp == L"" || temp[ 0 ] == L'#' ) continue;
		List_t< String^ > split;
		Utility::SplitString( %split, temp, L',' );
		if( split.Count < 5 )
		{
			//MessageBox::Show( L"Failed to load mycharms.txt: Wrong number of commas" );
			results->Clear();
			return results;
		}

		//slots,skill1,num,skill2,num,skill3,num
		Charm^ charm = gcnew Charm();
		charm->num_slots = Convert::ToUInt32( split[ 0 ] );

		try
		{
			for( unsigned i = 0; i < 3; ++i )
			{
				const int offset = i * 2 + 1;
				if( offset >= split.Count )
					break;

				if( split[ offset ] != L"" )
				{
					if( StringTable::english )
						split[ offset ] = FixTypos( split[ offset ] );
					Ability^ ab = Ability::FindAbility( split[ offset ] );
					if( !ab )
					{
						results->Clear();
						return results;
					}

					charm->abilities.Add( gcnew AbilityPair( ab, Convert::ToInt32( split[ offset + 1 ] ) ) );
				}
			}
		}
		catch( Exception^ )
		{
			results->Clear();
			return results;
		}
		results->Add( charm );
	}

	return results;
}

bool CharmDatabase::LoadCustom()
{
	if( !IO::File::Exists( CUSTOM_CHARM_TXT ) )
	{
		//Charm^ charm = gcnew Charm( 0 );
		//charm->abilities.Add( gcnew AbilityPair( SpecificAbility::gathering, 10 ) );
		mycharms.Clear();
		//mycharms.Add( charm );
		return true;
	}

	List_t< Charm^ >^ charms = LoadCharms( CUSTOM_CHARM_TXT );
	if( charms->Count == 0 )
		return true;

	mycharms.Clear();

	bool cheats = false;
	for each( Charm^ charm in charms )
	{
		if( CharmDatabase::CharmIsLegal( charm ) )
		{
			charm->custom = true;
			mycharms.Add( charm );
		}
		else
		{
			cheats = true;
		}
	}

	if( cheats )
		System::Windows::Forms::MessageBox::Show( StaticString( Cheater ) );

	return true;
}

#pragma endregion

#pragma region Charm Generation
ref struct StaticData
{
	static array< array< unsigned char >^ >^ skill1_table;
	static array< array<   signed char >^ >^ skill2_table; // [type][skill]
	static array< array< unsigned char >^ >^ slot_table;

	static array< unsigned char >^ skill2_chance_table =
	{
		100,
		35,
		25,
		20,
		15,
		25
	};
};

int rnd( const int n )
{
	Assert( n < 65536 && n >= 0, L"Bad RND" );
	if( n == 0 ) return 176;

	return ( n * 176 ) % 65363;
}

int reverse_rnd( const int n )
{
	return ( n * 7799 ) % 65363;
}

unsigned GetNumSlots( const unsigned charm_type, const int slot_table, const unsigned roll )
{
	Assert( (int)charm_type < StaticData::slot_table->Length, L"Bad charm type" );

	const unsigned table_index = Math::Min( slot_table, StaticData::slot_table[ charm_type ]->Length / 3 ) - 1;
	const unsigned for2 = StaticData::slot_table[ charm_type ][ table_index * 3 + 1 ];
	const unsigned for3 = StaticData::slot_table[ charm_type ][ table_index * 3 + 2 ];
	const unsigned for1 = StaticData::slot_table[ charm_type ][ table_index * 3 + 0 ];
	if( roll >= for2 )
	{
		return ( roll >= for3 ) ? 3 : 2;
	}
	else
	{
		return ( roll >= for1 ) ? 1 : 0;
	}
}

bool TryTwoSkillCharm( const unsigned charm_type, int n, int m, array< List_t< unsigned >^ >^ charms )
{
	array< unsigned char >^ skill1_table = StaticData::skill1_table[ charm_type ];
	array< signed char >^	skill2_table = StaticData::skill2_table[ charm_type ];
	const unsigned num_skills1 = skill1_table->Length / 3;
	const unsigned num_skills2 = skill2_table == nullptr ? 0 : skill2_table->Length / 3;

	//skill 1
	const int skill1_index = n % num_skills1;
	const int skill1_name = skill1_table[ skill1_index * 3 ];
	const int skill1_min = skill1_table[ skill1_index * 3 + 1 ];
	const int skill1_max = skill1_table[ skill1_index * 3 + 2 ];

	//skill 1 point
	n = rnd( n );
	const int point1 = skill1_min + (n^m) % ( skill1_max - skill1_min + 1 );

	//has skill 2?
	int skill2_index = -1, point2 = 0, skill2_min = 0, skill2_max = 0, skill2_name = 0;
	n = rnd( n );
	m = rnd( m );
	if( (n^m) % 100 >= StaticData::skill2_chance_table[ charm_type ] )
	{
		//skill 2
		m = rnd( m );
		skill2_index = m % num_skills2;
		skill2_name = skill2_table[ skill2_index * 3 ];
		skill2_min = skill2_table[ skill2_index * 3 + 1 ];
		skill2_max = skill2_table[ skill2_index * 3 + 2 ];

		//skill 2 point
		if( skill2_min > 0 ) //positive number
		{
			n = rnd( n );
			m = rnd( m );
			point2 = ( (n^m) % ( skill2_max - skill2_min + 1 ) ) + skill2_min;
		}
		else //check for negative
		{
			n = rnd( n );
			if( n % 2 == 1 ) //positive
			{
				n = rnd( n );
				m = rnd( m );
				point2 = (n^m) % skill2_max + 1;
			}
			else //negative
			{
				n = rnd( n );
				m = rnd( m );
				point2 = skill2_min + (n^m) % ( -skill2_min );
			}
		}
		if( point2 < 0 )
			return false;

		if( skill1_name == skill2_name )
		{
			skill2_min = skill2_max = point2 = 0;
			skill2_name = skill2_index = Ability::static_abilities.Count;
		}
	}

	const int slot_table = (int)Math::Floor( point1*10.0 / skill1_max + ( (point2 > 0) ? point2*10.0 / skill2_max : 0 ) );

	//slots
	n = rnd( n );
	int slot_roll = 0;
	if( n & 128 )
	{
		n = rnd( n );
		slot_roll = n % 100;
	}
	else
	{
		m = rnd( m );
		slot_roll = m % 100;
	}

	const int num_slots = GetNumSlots( charm_type, slot_table, slot_roll );

	List_t< unsigned >^ list = charms[ num_slots ];
	for( int i = 0; i < list->Count; ++i )
	{
		const unsigned hash = list[i];
		const int p1 = hash >> 16;
		const int p2 = hash & 0xFFFF;
		if( p1 >= point1 && p2 >= point2 )
			return false;
	}
	list->Add( point1 << 16 | point2 );

	return num_slots == Math::Min( 3u, charm_type + 1 ) && slot_table == 20;
}

String^ CanGenerateCharm1( const unsigned charm_type, int n, int m, Charm^ charm )
{
	Assert( charm->abilities.Count == 1, "Too many abilities for charm" );
	Assert( (int)charm_type < StaticData::skill1_table->Length && (int)charm_type < StaticData::skill2_table->Length, "Charm type is invalid" );
	array< unsigned char >^ skill1_table = StaticData::skill1_table[ charm_type ];
	array< signed char >^	skill2_table = StaticData::skill2_table[ charm_type ];
	const unsigned num_skills1 = skill1_table->Length / 3;
	const unsigned num_skills2 = skill2_table->Length / 3;

	//skill 1
	const int skill1_index = n % num_skills1;
	const int skill1_name = skill1_table[ skill1_index * 3 ];
	const int skill1_min = skill1_table[ skill1_index * 3 + 1 ];
	const int skill1_max = skill1_table[ skill1_index * 3 + 2 ];

	//skill 1 point
	n = rnd( n );
	const int point1 = skill1_min + (n^m) % ( skill1_max - skill1_min + 1 );

	if( skill1_name == charm->abilities[0]->ability->static_index && point1 < charm->abilities[0]->amount )
		return nullptr;

	//has skill 2?
	int skill2_index = -1, point2 = 0, skill2_min = 0, skill2_max = 0, skill2_name = 0;
	n = rnd( n );
	m = rnd( m );
	if( (n^m) % 100 >= StaticData::skill2_chance_table[ charm_type ] )
	{
		//skill 2
		m = rnd( m );
		skill2_index = m % num_skills2;
		skill2_name = skill2_table[ skill2_index * 3 ];
		skill2_min = skill2_table[ skill2_index * 3 + 1 ];
		skill2_max = skill2_table[ skill2_index * 3 + 2 ];

		//skill 2 point
		if( skill2_min > 0 ) //positive number
		{
			n = rnd( n );
			m = rnd( m );
			point2 = ( (n^m) % ( skill2_max - skill2_min + 1 ) ) + skill2_min;
		}
		else //check for negative
		{
			n = rnd( n );
			if( n % 2 == 1 ) //positive
			{
				n = rnd( n );
				m = rnd( m );
				point2 = (n^m) % skill2_max + 1;
			}
			else //negative
			{
				n = rnd( n );
				m = rnd( m );
				point2 = skill2_min + (n^m) % ( -skill2_min );
			}
		}

		if( skill1_name == skill2_name )
		{
			skill2_name = point2 = 0;
			skill2_index = -1;
		}

		if( skill1_name != charm->abilities[0]->ability->static_index && skill2_name != charm->abilities[0]->ability->static_index ||
			point2 < charm->abilities[0]->amount )
			return nullptr;
	}

	const int slot_table = (int)Math::Floor( point1*10.0 / skill1_max + ( (point2 > 0) ? point2*10.0 / skill2_max : 0 ) );

	//slots
	n = rnd( n );
	int slot_roll = 0;
	if( n & 128 )
	{
		n = rnd( n );
		slot_roll = n % 100;
	}
	else
	{
		m = rnd( m );
		slot_roll = m % 100;
	}

	const unsigned num_slots = GetNumSlots( charm_type, slot_table, slot_roll );

	if( num_slots < charm->num_slots )
		return nullptr;

	Charm c( num_slots );
	c.abilities.Add( gcnew AbilityPair( Ability::static_abilities[ skill1_name ], point1 ) );
	if( point2 )
		c.abilities.Add( gcnew AbilityPair( Ability::static_abilities[ skill2_name ], point2 ) );

	return c.GetName();
}

String^ CanGenerateCharm2( const unsigned charm_type, int n, int m, Charm^ charm )
{
	Assert( charm->abilities.Count == 2, "Too few abilities for charm" );
	Assert( (int)charm_type < StaticData::skill1_table->Length && (int)charm_type < StaticData::skill2_table->Length, "Charm type is invalid" );
	array< unsigned char >^ skill1_table = StaticData::skill1_table[ charm_type ];
	array< signed char >^	skill2_table = StaticData::skill2_table[ charm_type ];
	const unsigned num_skills1 = skill1_table->Length / 3;
	const unsigned num_skills2 = skill2_table->Length / 3;

	//skill 1
	const int skill1_index = n % num_skills1;
	const int skill1_name = skill1_table[ skill1_index * 3 ];
	const int skill1_min = skill1_table[ skill1_index * 3 + 1 ];
	const int skill1_max = skill1_table[ skill1_index * 3 + 2 ];

	//skill 1 point
	n = rnd( n );
	const int point1 = skill1_min + (n^m) % ( skill1_max - skill1_min + 1 );

	if( point1 < charm->abilities[0]->amount )
		return nullptr;

	//has skill 2?
	int skill2_index = -1, point2 = 0, skill2_min = 0, skill2_max = 0, skill2_name = 0;
	n = rnd( n );
	m = rnd( m );
	if( (n^m) % 100 >= StaticData::skill2_chance_table[ charm_type ] )
	{
		//skill 2
		m = rnd( m );
		skill2_index = m % num_skills2;
		skill2_name = skill2_table[ skill2_index * 3 ];
		skill2_min = skill2_table[ skill2_index * 3 + 1 ];
		skill2_max = skill2_table[ skill2_index * 3 + 2 ];

		//skill 2 point
		if( skill2_min > 0 ) //positive number
		{
			n = rnd( n );
			m = rnd( m );
			point2 = ( (n^m) % ( skill2_max - skill2_min + 1 ) ) + skill2_min;
		}
		else //check for negative
		{
			n = rnd( n );
			if( n % 2 == 1 ) //positive
			{
				n = rnd( n );
				m = rnd( m );
				point2 = (n^m) % skill2_max + 1;
			}
			else //negative
			{
				n = rnd( n );
				m = rnd( m );
				point2 = skill2_min + (n^m) % ( -skill2_min );
			}
		}

		if( skill1_name == skill2_name || point2 < charm->abilities[1]->amount )
			return nullptr;
	}
	else return nullptr;

	const int slot_table = (int)Math::Floor( point1*10.0 / skill1_max + ( (point2 > 0) ? point2*10.0 / skill2_max : 0 ) );

	//slots
	n = rnd( n );
	int slot_roll = 0;
	if( n & 128 )
	{
		n = rnd( n );
		slot_roll = n % 100;
	}
	else
	{
		m = rnd( m );
		slot_roll = m % 100;
	}

	const unsigned num_slots = GetNumSlots( charm_type, slot_table, slot_roll );

	if( num_slots < charm->num_slots )
		return nullptr;

	Charm c( num_slots );
	c.abilities.Add( gcnew AbilityPair( Ability::static_abilities[ skill1_name ], point1 ) );
	c.abilities.Add( gcnew AbilityPair( Ability::static_abilities[ skill2_name ], point2 ) );

	return c.GetName();
}

bool CanGenerate2SkillCharm( const unsigned charm_type, int n, int m, Charm^ charm )
{
	Assert( charm->abilities.Count == 2, "Charm has too few abilities" );
	Assert( charm_type > 0 && (int)charm_type < StaticData::skill1_table->Length && (int)charm_type < StaticData::skill2_table->Length, "Charm type is invalid" );
	array< unsigned char >^ skill1_table = StaticData::skill1_table[ charm_type ];
	array< signed char >^	skill2_table = StaticData::skill2_table[ charm_type ];
	const unsigned num_skills1 = skill1_table->Length / 3;
	const unsigned num_skills2 = skill2_table->Length / 3;

	//skill 1
	//n = rnd( n );
	const int skill1_index = n % num_skills1;
	const int skill1_name = skill1_table[ skill1_index * 3 ];
	Assert( skill1_name == charm->abilities[ 0 ]->ability->static_index, "Skill1 failed" );
	const int skill1_min = skill1_table[ skill1_index * 3 + 1 ];
	const int skill1_max = skill1_table[ skill1_index * 3 + 2 ];

	//skill 1 point
	n = rnd( n );
	//m = rnd( m );
	const int point1 = skill1_min + (n^m) % ( skill1_max - skill1_min + 1 );

	if( point1 < charm->abilities[0]->amount )
		return false;

	//has skill 2?
	int skill2_index = -1, point2 = 0, skill2_min = 0, skill2_max = 0, skill2_name = 0;
	n = rnd( n );
	m = rnd( m );
	if( (n^m) % 100 >= StaticData::skill2_chance_table[ charm_type ] )
	{
		//skill 2
		m = rnd( m );
		skill2_index = m % num_skills2;
		skill2_name = (unsigned char)skill2_table[ skill2_index * 3 ];
		Assert( skill2_name == charm->abilities[1]->ability->static_index, "Skill2 failed" );
		skill2_min = skill2_table[ skill2_index * 3 + 1 ];
		skill2_max = skill2_table[ skill2_index * 3 + 2 ];

		//skill 2 point
		if( skill2_min > 0 ) //positive number
		{
			n = rnd( n );
			m = rnd( m );
			point2 = ( (n^m) % ( skill2_max - skill2_min + 1 ) ) + skill2_min;
		}
		else //check for negative
		{
			n = rnd( n );
			if( n % 2 == 1 ) //positive
			{
				n = rnd( n );
				m = rnd( m );
				point2 = (n^m) % skill2_max + 1;
			}
			else //negative
			{
				n = rnd( n );
				m = rnd( m );
				point2 = skill2_min + (n^m) % ( -skill2_min );
			}
		}

		if( skill1_name == skill2_name || point2 < charm->abilities[1]->amount )
		{
			return false;
		}
	}
	else return false;

	const int slot_table = (int)Math::Floor( point1*10.0 / skill1_max + ( (point2 > 0) ? point2*10.0 / skill2_max : 0 ) );

	//slots
	n = rnd( n );
	int slot_roll = 0;
	if( n & 128 )
	{
		n = rnd( n );
		slot_roll = n % 100;
	}
	else
	{
		m = rnd( m );
		slot_roll = m % 100;
	}

	const unsigned num_slots = GetNumSlots( charm_type, slot_table, slot_roll );

	return num_slots >= charm->num_slots;
}

bool GenerateCharm3( const unsigned charm_type, const unsigned table, int n, int m, Charm^ charm )
{
	//check charm_type < StaticData::skill1_table->Length
	array< unsigned char >^ skill1_table = StaticData::skill1_table[ charm_type ];
	const unsigned num_skills1 = skill1_table->Length / 3;
	//check charm_type < StaticData::skill2_table->Length
	array< signed char >^ skill2_table = StaticData::skill2_table[ charm_type ];
	const unsigned num_skills2 = skill2_table == nullptr ? 0 : skill2_table->Length / 3;

	//skill 1
	//n = rnd( n );
	const int skill1_index = n % num_skills1;
	const int skill1_name = skill1_table[ skill1_index * 3 ];
	const int skill1_min = skill1_table[ skill1_index * 3 + 1 ];
	const int skill1_max = skill1_table[ skill1_index * 3 + 2 ];

	//skill 1 point
	n = rnd( n );
	//m = rnd( m );
	const int point1 = skill1_min + (n^m) % ( skill1_max - skill1_min + 1 );

	//has skill 2?
	int skill2_index = -1, point2 = 0, skill2_min = 0, skill2_max = 0, skill2_name = 0;
	n = rnd( n );
	m = rnd( m );
	if( (n^m) % 100 >= StaticData::skill2_chance_table[ charm_type ] )
	{
		//skill 2
		m = rnd( m );
		skill2_index = m % num_skills2;
		skill2_name = skill2_table[ skill2_index * 3 ];
		skill2_min = skill2_table[ skill2_index * 3 + 1 ];
		skill2_max = skill2_table[ skill2_index * 3 + 2 ];

		//skill 2 point
		if( skill2_min > 0 ) //positive number
		{
			n = rnd( n );
			m = rnd( m );
			point2 = ( (n^m) % ( skill2_max - skill2_min + 1 ) ) + skill2_min;
		}
		else //check for negative
		{
			n = rnd( n );
			if( n % 2 == 1 ) //positive
			{
				n = rnd( n );
				m = rnd( m );
				point2 = (n^m) % skill2_max + 1;
			}
			else //negative
			{
				n = rnd( n );
				m = rnd( m );
				point2 = skill2_min + (n^m) % ( -skill2_min );
			}
		}		

		if( skill1_name == skill2_name )
		{
			skill2_min = skill2_max = point2 = 0;
			skill2_name = skill2_index = Ability::static_abilities.Count;
		}
	}

	const int slot_table = (int)Math::Floor( point1*10.0 / skill1_max + ( (point2 > 0) ? point2*10.0 / skill2_max : 0 ) );

	//slots
	n = rnd( n );
	int slot_roll = 0;
	if( n & 128 )
	{
		n = rnd( n );
		slot_roll = n % 100;
	}
	else
	{
		m = rnd( m );
		slot_roll = m % 100;
	}

	const int num_slots = GetNumSlots( charm_type, slot_table, slot_roll );

	if( num_slots != charm->num_slots )
		return false;

	if( point2 == 0 )
	{
		if( charm->abilities.Count == 2 ||
			skill1_name != charm->abilities[ 0 ]->ability->static_index ||
			point1 != charm->abilities[ 0 ]->amount )
			return false;
	}
	else
	{
		if( charm->abilities.Count != 2 ||
			skill1_name != charm->abilities[ 0 ]->ability->static_index ||
			skill2_name != charm->abilities[ 1 ]->ability->static_index ||
			point1 != charm->abilities[ 0 ]->amount ||
			point2 != charm->abilities[ 1 ]->amount )
			return false;
	}
	return true;
}

array< unsigned char >^ LoadSlotTable( String^ filename )
{
	array< String^ >^ lines = IO::File::ReadAllLines( filename );
	array< unsigned char >^ result = gcnew array< unsigned char >( lines->Length * 3 - 3 );
	for( int i = 1, index = 0; i < lines->Length; ++i )
	{
		array< String^ >^ tokens = lines[ i ]->Split( ',' );
		for( int j = 1; j < tokens->Length; ++j )
			result[ index++ ] = (unsigned char)Convert::ToUInt16( tokens[ j ] );
	}
	return result;
}

template< typename T >
array< T >^ LoadSkillTable( String^ filename )
{
	array< String^ >^ lines = IO::File::ReadAllLines( filename );
	array< T >^ result = gcnew array< T >( lines->Length * 3 - 3 );
	for( int i = 1, index = 0; i < lines->Length; ++i )
	{
		array< String^ >^ tokens = lines[ i ]->Split( ',' );
		
		Ability^ ab = Ability::FindCharmAbility( tokens[ 0 ] );
		result[ index++ ] = ab->static_index;
		result[ index++ ] = (T)Convert::ToInt16( tokens[ 1 ] );
		result[ index++ ] = (T)Convert::ToInt16( tokens[ 2 ] );
	}
	return result;
}

void LoadCharmTableData()
{
	array< String^ >^ charm_names =
	{
		"mystery",
		"shining",
		"ancient",
	};

	const unsigned NumCharmTypes = charm_names->Length;

	StaticData::skill1_table = gcnew array< array< unsigned char >^ >( NumCharmTypes );
	StaticData::skill2_table = gcnew array< array< signed char >^ >( NumCharmTypes );
	StaticData::slot_table = gcnew array< array< unsigned char >^ >( NumCharmTypes );

	for( unsigned i = 0; i < NumCharmTypes; ++i )
	{
		StaticData::slot_table[ i ] = LoadSlotTable( "Data/Charm Generation/" + charm_names[ i ] + "_slots.csv" );
		StaticData::skill1_table[ i ] = LoadSkillTable< unsigned char >( "Data/Charm Generation/" + charm_names[ i ] + "_skill1.csv" );
		if( i > 0 )
			StaticData::skill2_table[ i ] = LoadSkillTable< signed char >( "Data/Charm Generation/" + charm_names[ i ] + "_skill2.csv" );
	}
}

unsigned char GetMaxSingleSkill( const int index, const unsigned charm_type )
{
	unsigned char res = 0;
	for( int i = 0; i < StaticData::skill1_table[ charm_type ]->Length; i += 3 )
	{
		if( StaticData::skill1_table[ charm_type ][ i ] == index )
			res = Math::Max( res, StaticData::skill1_table[ charm_type ][ i + 2 ] );
	}
	if( StaticData::skill2_table[ charm_type ] )
	{
		for( int i = 0; i < StaticData::skill2_table[ charm_type ]->Length; i += 3 )
		{
			if( StaticData::skill2_table[ charm_type ][ i ] == index )
				res = Math::Max( res, (unsigned char)StaticData::skill2_table[ charm_type ][ i + 2 ] );
		}
	}
	return res;
}

void SetupSingleSkillMaxs()
{
	for( int i = 0; i < Ability::static_abilities.Count; ++i )
	{
		Ability^ a = Ability::static_abilities[ i ];
		a->max_vals1 = gcnew array< unsigned char >( CharmDatabase::NumCharmTypes );
		for( unsigned charm_type = 0; charm_type < CharmDatabase::NumCharmTypes; charm_type++ )
		{
			a->max_vals1[ charm_type ]= GetMaxSingleSkill( a->static_index, charm_type );
		}
	}
}

void CreateTableSeedList()
{
	CharmDatabase::table_seed_list = gcnew array< List_t< unsigned short >^ >( CharmDatabase::table_seeds->Length );
	for( int i = 0; i < CharmDatabase::table_seeds->Length; ++i )
	{
		CharmDatabase::table_seed_list[ i ] = gcnew List_t< unsigned short >();

		int n = CharmDatabase::table_seeds[ i ];
		do 
		{
			CharmDatabase::table_seed_list[ i ]->Add( (unsigned short)(n & 0xFFFF) );
			n = rnd( n );
		}
		while( n != CharmDatabase::table_seeds[ i ] );

		CharmDatabase::table_seed_list[ i ]->Sort();
	}
}

int FindTable( const int n )
{
	for( int i = 0; i < CharmDatabase::table_seed_list->Length; ++i )
	{
		if( CharmDatabase::table_seed_list[i]->BinarySearch( n ) >= 0 )
			return i;
	}
	return -1;
}

/*void temp()
{
	array< String^ >^ skill_names = { L"なし", L"胴系統倍化", L"毒", L"麻痺", L"睡眠", L"気絶", L"耐泥耐雪", L"対防御DOWN", L"細菌学", L"攻撃", L"防御", L"体力", L"回復速度", L"加護", L"納刀", L"溜め短縮", L"達人", L"痛撃", L"重撃", L"KO", L"減気攻撃", L"体術", L"聴覚保護", L"風圧", L"耐震", L"耐暑", L"耐寒", L"スタミナ", L"気力回復", L"回避性能", L"回避距離", L"気配", L"ガード性能", L"ガード強化", L"特殊攻撃", L"属性攻撃", L"火属性攻撃", L"水属性攻撃", L"雷属性攻撃", L"氷属性攻撃", L"龍属性攻撃", L"火耐性", L"水耐性", L"雷耐性", L"氷耐性", L"龍耐性", L"斬れ味", L"匠", L"研ぎ師", L"抜刀減気", L"抜刀会心", L"剣術", L"装填速度", L"装填数", L"速射", L"反動", L"精密射撃", L"通常弾強化", L"貫通弾強化", L"散弾強化", L"通常弾追加", L"貫通弾追加", L"散弾追加", L"榴弾追加", L"拡散弾追加", L"斬裂弾追加", L"爆破弾追加", L"毒瓶追加", L"麻痺瓶追加", L"睡眠瓶追加", L"強撃瓶追加", L"接撃瓶追加", L"減気瓶追加", L"爆破瓶追加", L"笛", L"砲術", L"爆弾強化", L"回復量", L"広域", L"効果持続", L"腹減り", L"食いしん坊", L"食事", L"肉食", L"調合成功率", L"調合数", L"採取", L"高速収集", L"気まぐれ", L"ハチミツ", L"護石王", L"運気", L"剥ぎ取り", L"捕獲", L"観察眼", L"千里眼", L"運搬", L"狩人", L"盗み無効", L"高速設置", L"燃鱗", L"底力", L"逆境", L"本気", L"闘魂", L"無傷", L"属性解放", L"根性", L"采配", L"号令", L"自動防御", L"属性耐性", L"状態耐性", L"怒", L"刀匠", L"射手", L"不動", L"一心", L"頑強", L"強欲", L"狂撃耐性", L"剛撃", L"指揮", L"乗り", L"居合", L"回避術", L"盾持", L"潔癖", L"護石収集", L"増幅", L"裂傷", L"節食", L"茸食", L"食欲", L"耐粘", L"射法", L"裏稼業", L"斬術", L"特殊会心", L"属性会心", L"北辰納豆流", L"秘伝", L"職工", L"剛腕", L"祈願" };
	array< array< int >^ >^ vals = { { 2, 1, 5, 1, 5, -10, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, 10 }, { 3, 1, 5, 1, 5, -10, 7 }, { 4, 1, 5, 1, 5, -10, 7 }, { 5, 1, 8, 1, 8, -10, 10, 0, 0, -10, 13, 0, 0, -8, 11, 0, 0, -5, 14 }, { 6, 1, 7, 1, 7, -10, 8 }, { 7, 1, 7, 1, 7, -10, 8 }, { 8 }, { 9, 1, 4, 1, 4, -7, 7, 0, 0, -10, 10, 0, 0, -8, 14, 0, 0, -3, 14, 0, 0, 2, 14 }, { 10, 1, 4, 1, 4, -7, 7, 0, 0, -10, 10, 0, 0, -8, 10, 0, 0, -3, 10, 0, 0, 2, 10 }, { 11, 1, 8, 1, 8, -10, 10, 0, 0, -10, 13, 0, 0, -8, 11, 0, 0, -3, 11, 0, 0, 3, 11 }, { 12, 0, 0, 1, 7, -4, 4, 0, 0, -10, 12, 0, 0, -8, 11, 0, 0, -3, 12 }, { 13, 0, 0, 1, 7, 0, 0, 1, 7, -10, 9, 3, 7, -8, 9, 4, 8, -5, 9, 0, 0, 2, 9 }, { 14, 0, 0, 1, 6, 0, 0, 1, 6, -10, 4, 3, 6, -8, 4, 3, 8, -5, 4, 0, 0, 2, 4 }, { 15, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 5, 3, 6, 1, 5 }, { 16, 1, 4, 1, 4, -7, 7, 0, 0, -10, 10, 0, 0, -8, 14, 0, 0, -5, 14, 0, 0, 3, 14 }, { 17, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 5, 4, 6, 2, 5 }, { 18, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 5, 3, 8, 2, 5 }, { 19, 0, 0, 1, 6, 0, 0, 1, 6, -3, 4, 3, 6, -1, 4, 3, 8, -1, 6, 0, 0, 2, 5 }, { 20, 0, 0, 1, 6, 0, 0, 1, 6, -3, 4, 3, 6, -1, 4, 3, 8, -1, 6, 0, 0, 2, 6 }, { 21, 0, 0, 1, 6, 0, 0, 1, 6, -3, 4, 3, 6, -1, 4, 3, 8, -1, 6, 0, 0, 2, 5 }, { 22, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 5, 3, 8, 1, 5 }, { 23, 0, 0, 1, 4, 0, 0, 1, 5, -10, 3, 3, 5, -8, 3, 3, 6, -5, 4, 0, 0, 2, 4 }, { 24, 1, 7, 1, 7, -10, 8 }, { 25, 1, 10, 0, 0, -10, 10 }, { 26, 1, 10, 0, 0, -10, 10 }, { 27, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 5, 3, 8, 2, 5 }, { 28, 0, 0, 1, 6, 0, 0, 1, 6, -3, 4, 3, 6, -1, 4, 3, 8, -1, 6, 0, 0, 2, 5 }, { 29, 0, 0, 1, 6, 0, 0, 1, 6, -3, 4, 3, 6, -1, 9, 3, 6, -1, 9, 0, 0, 2, 9 }, { 30, 0, 0, 0, 0, 0, 0, 1, 6, -3, 4, 3, 6, -1, 4, 3, 8, -1, 6, 0, 0, 2, 6 }, { 31, 1, 8, 0, 0, -10, 10 }, { 32, 0, 0, 1, 6, 0, 0, 1, 6, -3, 4, 3, 6, -1, 4, 3, 8, -1, 6, 0, 0, 2, 6 }, { 33, 0, 0, 1, 4, 0, 0, 1, 6, -3, 4, 3, 6, -1, 4, 3, 8, -1, 6, 0, 0, 2, 6 }, { 34, 0, 0, 1, 4, -4, 4, 1, 6, -10, 4, 3, 6, -8, 4, 3, 8, -5, 4, 0, 0, 2, 4 }, { 35, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 2, 5, -2, 6, 2, 6, 1, 4 }, { 36, 0, 0, 1, 7, 0, 0, 1, 7, -10, 13, 3, 7, -8, 10, 4, 8, -3, 14 }, { 37, 0, 0, 1, 7, 0, 0, 1, 7, -10, 13, 3, 7, -8, 10, 4, 8, -3, 14 }, { 38, 0, 0, 1, 7, 0, 0, 1, 7, -10, 13, 3, 7, -8, 10, 4, 8, -3, 14 }, { 39, 0, 0, 1, 7, 0, 0, 1, 7, -10, 13, 3, 7, -8, 10, 4, 8, -3, 14 }, { 40, 0, 0, 1, 7, 0, 0, 1, 7, -10, 13, 3, 7, -8, 10, 4, 8, -3, 14 }, { 41, 1, 6, 1, 6, -10, 10, 0, 0, -10, 13, 0, 0, -8, 11, 0, 0, -8, 14 }, { 42, 1, 6, 1, 6, -10, 10, 0, 0, -10, 13, 0, 0, -8, 11, 0, 0, -8, 14 }, { 43, 1, 6, 1, 6, -10, 10, 0, 0, -10, 13, 0, 0, -8, 11, 0, 0, -8, 14 }, { 44, 1, 6, 1, 6, -10, 10, 0, 0, -10, 13, 0, 0, -8, 11, 0, 0, -8, 14 }, { 45, 1, 6, 1, 6, -10, 10, 0, 0, -10, 13, 0, 0, -8, 11, 0, 0, -8, 14 }, { 46, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 5, 3, 8, 1, 5 }, { 47, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 2, 5, -2, 7, 2, 6, 1, 4 }, { 48, 0, 0, 1, 4, 0, 0, 0, 0, -10, 8, 0, 0, -8, 8, 0, 0, -3, 10, 0, 0, 2, 8 }, { 49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 2, 5, -2, 7, 0, 0, 2, 7 }, { 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 2, 5, -2, 7, 0, 0, 2, 7 }, { 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 2, 5, -2, 7, 2, 6 }, { 52, 0, 0, 1, 4, 0, 0, 1, 6, -3, 4, 3, 6, -1, 4, 3, 8, -1, 6, 0, 0, 2, 6 }, { 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 2, 5, -2, 7, 2, 6, 1, 4 }, { 54, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 2, 5, -2, 4, 0, 0, 1, 6 }, { 55, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 5, 4, 6, 2, 5 }, { 56, 1, 6, 1, 6, -10, 10 }, { 57, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 8, 0, 0, 2, 8 }, { 58, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 8, 0, 0, 2, 8 }, { 59, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 8, 0, 0, 2, 8 }, { 60, 1, 6, 1, 6, -8, 8 }, { 61, 1, 6, 1, 6, -10, 10 }, { 62, 1, 6, 1, 6, -10, 10 }, { 63, 1, 6, 1, 6, -10, 10 }, { 64, 1, 6, 1, 6, -10, 10 }, { 65, 1, 6, 1, 6, -10, 10 }, { 66, 1, 6, 1, 6, -10, 10 }, { 67, 1, 8, 0, 0, -10, 10 }, { 68, 1, 7, 0, 0, -10, 10 }, { 69, 1, 8, 0, 0, -10, 10 }, { 70, 1, 7, 0, 0, -10, 10 }, { 71, 1, 8, 0, 0, -10, 10 }, { 72, 1, 8, 0, 0, -10, 10 }, { 73, 1, 6, 1, 6, -10, 10 }, { 74, 1, 6, 1, 6, -8, 8, 0, 0, -10, 10, 0, 0, -8, 10, 0, 0, -5, 10, 0, 0, 4, 10 }, { 75, 1, 6, 1, 6, -8, 8, 0, 0, -10, 10, 0, 0, -8, 14, 0, 0, -5, 14, 0, 0, 3, 14 }, { 76, 1, 6, 1, 6, -8, 8, 0, 0, -10, 10, 0, 0, -8, 10, 0, 0, -3, 10, 0, 0, 3, 10 }, { 77, 0, 0, 1, 6, 0, 0, 1, 6, -3, 4, 3, 6, -1, 4, 3, 8, -1, 6, 0, 0, 2, 6 }, { 78, 1, 8, 1, 8, -10, 10, 0, 0, -10, 12, 0, 0, -8, 10, 0, 0, -5, 12, 0, 0, 4, 10 }, { 79, 1, 8, 0, 0, -10, 10 }, { 80, 1, 8, 1, 8, -10, 10, 0, 0, -9, 10, 0, 0, -10, 10 }, { 81, 1, 10, 0, 0, -10, 13 }, { 82, 0, 0, 1, 4, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 5, 0, 0, 2, 4 }, { 83, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, -5, 5, 2, 5, -2, 4 }, { 84, 1, 10, 0, 0, -10, 13 }, { 85, 1, 8, 0, 0, -10, 10 }, { 86, 1, 10, 0, 0, -10, 13 }, { 87, 1, 8, 0, 0, -10, 10 }, { 88, 1, 10, 0, 0, -10, 13 }, { 89, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, -5, 5 }, { 90, 0, 0, 1, 3, -5, 5, 3, 7, -8, 10 }, { 91, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -5, 7, 2, 5, -5, 7, 2, 6, 1, 7 }, { 92, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -5, 7, 2, 5, -5, 7, 2, 6, 1, 7 }, { 93, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -5, 7, 2, 5, -5, 7, 2, 6, 1, 7 }, { 94, 1, 8, 0, 0, -10, 10 }, { 95, 1, 8, 1, 8, -10, 10, 0, 0, -10, 12, 0, 0, -8, 10, 0, 0, -5, 14 }, { 96, 1, 8, 1, 8, -10, 10 }, { 97, 1, 8, 0, 0, -10, 10 }, { 98, 1, 10, 0, 0, -10, 10 }, { 99, 1, 8, 1, 8, -10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 10 }, { 100, 1, 3, 1, 5, -7, 7, 1, 7, -10, 10, 3, 7, 1, 10 }, { 101, 0, 0, 1, 6, 0, 0, 1, 6, -3, 4, 3, 6, -1, 4, 3, 8, -1, 4, 0, 0, 2, 4 }, { 102, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 2, 5, -2, 4, 2, 6, 1, 4 }, { 103, 0, 0, 0, 0, 0, 0, 1, 6, -3, 4, 3, 6, -1, 4, 3, 8, -1, 6, 4, 7, 2, 6 }, { 104, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, -3, 3, 2, 5, -2, 6, 2, 4, 1, 4 }, { 105, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, -3, 3, 2, 5, -2, 4, 2, 6, 1, 4 }, { 106, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, -5, 7, 2, 5, -2, 7, 2, 6 }, { 107, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 2, 5, -2, 4, 2, 6, 1, 4 }, { 108, 1, 3, 1, 5, -7, 7, 1, 7, -10, 10, 3, 7, 1, 10, 4, 8, 5, 10 }, { 109, 1, 3, 1, 5, -7, 7, 1, 7, -10, 10, 3, 7, 1, 10, 4, 8, 5, 10 }, { 110 }, { 111, 0, 0, 0, 0, 0, 0, 1, 5, -3, 3, 3, 5, -1, 3, 3, 6, -1, 5, 4, 8, 1, 5 }, { 112, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 3, 0, 0, 0, 0, 1, 4 }, { 113, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 3, 0, 0, 0, 0, 1, 4 }, { 114, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 3, 0, 0, 0, 0, 1, 4 }, { 115, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 3, 0, 0, 0, 0, 1, 4 }, { 116, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, 3, 0, 0, 0, 0, 1, 4, 1, 4 }, { 117, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4 }, { 118, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4 }, { 119, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6 }, { 120, 1, 3, 0, 0, -5, 5, 1, 5, 0, 0, 1, 7, 0, 0, 2, 7 }, { 121, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4 }, { 122 }, { 123, 1, 10, 0, 0, -8, 8, 0, 0, -5, 5, 1, 8, 0, 0, 2, 8 }, { 124, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4 }, { 125, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4 }, { 126, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4 }, { 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4 }, { 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 6 }, { 129, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4 }, { 130 }, { 131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 8, 3, 6 }, { 132, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 8, 3, 6 }, { 133 }, { 134 }, { 135, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 2, 5 }, { 136, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 1, 3 }, { 137, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 1, 3 }, { 138, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 2, 7, 0, 0, 2, 7 }, { 139, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 2, 7, 0, 0, 2, 7 }, { 140 }, { 141, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 1, 3 }, { 142 }, { 143, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 1, 3 }, { 144, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 6, 2, 5 } };

	//0: name
	//1-2: mystery
	//3-6: shining
	//7-10: timeworn
	//11-14: weathered
	//15-18: carved
		
	System::Collections::Generic::List< String^ > output1, output2;
	output1.Add(L"スキル系統,最小,最大");
	output2.Add(L"スキル系統,最小,最大");
	for( int i = 0; i < vals->Length; ++i )
	{
		array< int >^ s = vals[ i ];
		if( s->Length < 19 )
			continue;

		if( s[16] > 0 )
			output1.Add( skill_names[ s[0] ] + L"," + Convert::ToString(s[15]) + L"," + Convert::ToString(s[16]) );

		if( s[18] > 0 )
			output2.Add( skill_names[ s[0] ] + L"," + Convert::ToString(s[17]) + L"," + Convert::ToString(s[18]) );
	}

	IO::File::WriteAllLines( L"carved_skill1.csv", output1.ToArray() );
	IO::File::WriteAllLines( L"carved_skill2.csv", output2.ToArray() );
}*/

void CharmDatabase::GenerateCharmTable()
{
	return; //TODO?

	location_cache = gcnew Map_t< System::String^, CharmLocationDatum^ >();
	LoadCharmTableData();
	CreateTableSeedList();
	SetupSingleSkillMaxs();

	return;
}

#pragma endregion

bool CanFind2SkillCharm( Charm^ charm )
{
	if( charm->abilities.Count < 2 )
		return false;

	const unsigned start = ( charm->num_slots == 3 ) ? 2 : 1;

	array< int >^ skill1_index = gcnew array< int >( CharmDatabase::NumCharmTypes );
	array< int >^ skill2_index = gcnew array< int >( CharmDatabase::NumCharmTypes );

	//quick check first
	for( unsigned charm_type = start; charm_type < CharmDatabase::NumCharmTypes; ++charm_type )
	{
		skill1_index[ charm_type ] = -1;
		skill2_index[ charm_type ] = -1;

		array< unsigned char >^ t1 = StaticData::skill1_table[ charm_type ];
		bool t1_tight = false;
		for( int i = 0; i < t1->Length; i += 3 )
		{
			if( t1[i] == charm->abilities[0]->ability->static_index )
			{
				if( charm->abilities[0]->amount <= t1[i+2] )
					skill1_index[ charm_type ] = i;
				t1_tight = charm->abilities[0]->amount == t1[i+2];
				break;
			}
		}
		if( skill1_index[ charm_type ] == -1 )
			continue;

		array< signed char >^ t2 = StaticData::skill2_table[ charm_type ];
		bool t2_tight = false;
		for( int i = 0; i < t2->Length; i += 3 )
		{
			if( (unsigned char)t2[i] == charm->abilities[1]->ability->static_index )
			{
				if( charm->abilities[1]->amount <= t2[i+2] )
					skill2_index[ charm_type ] = i;
				t2_tight = charm->abilities[1]->amount == t2[i+2];
				break;
			}
		}
		if( skill2_index[ charm_type ] == -1 )
			continue;

		if( charm->num_slots == 0 )
			return true;

		if( !t1_tight && !t2_tight )
			return true;
	}

	//slow check if needed

	for( unsigned charm_type = start; charm_type < CharmDatabase::NumCharmTypes; ++charm_type )
	{
		if( skill1_index[ charm_type ] < 0 ||
			skill2_index[ charm_type ] < 0 )
			continue;

		const unsigned skill1_table_index = skill1_index[ charm_type ];
		const unsigned skill2_table_index = skill2_index[ charm_type ];
		
		const unsigned num1 = StaticData::skill1_table[ charm_type ]->Length / 3;
		const unsigned num2 = StaticData::skill2_table[ charm_type ]->Length / 3;

		for( int n = skill1_table_index / 3; n < 65363; n += num1 )
		{
			for( int m = skill2_table_index / 3; m < 65363; m += num2 )
			{
				const int initm = reverse_rnd(reverse_rnd(m == 0 ? num2 : m));
				if( CanGenerate2SkillCharm( charm_type, n, initm, charm ) )
					return true;
			}
		}
	}

	return false;
}

bool CharmDatabase::CharmIsLegal( Charm^ charm )
{
	if( charm->num_slots > 3 )
		return false;

	return true; //TODO: fix
	/*else */if( charm->abilities.Count == 0 )
		return true;
	else if( charm->abilities.Count == 1 )
	{
		AbilityPair^ ap = charm->abilities[0];
		const unsigned start[4] =
		{
			0,
			0,
			1,
			2
		};
		for( unsigned c = start[ charm->num_slots ]; c < CharmDatabase::NumCharmTypes; ++c )
			if( ap->amount <= ap->ability->max_vals1[c] )
				return true;
	}
	else if( charm->abilities.Count == 2 )
	{
		if( !CanFind2SkillCharm( charm ) )
		{
			AbilityPair^ temp = charm->abilities[0];
			charm->abilities[0] = charm->abilities[1];
			charm->abilities[1] = temp;
			if( CanFind2SkillCharm( charm ) )
			{
				temp = charm->abilities[0];
				charm->abilities[0] = charm->abilities[1];
				charm->abilities[1] = temp;
				return true;
			}
			else return false;
		}
		else return true;
	}
	return false;
}

void FindTwoSkillCharms( List_t< unsigned >^ charms, const int n0, const int m0, const int num1, const int num2, const unsigned t )
{
	return;

	for( int n = n0; n < 65363; n += num1 )
	{
		for( int m = m0; m < 65363; m += num2 )
		{
			const int initm = reverse_rnd(reverse_rnd(m == 0 ? num2 : m));
			//if( TryTwoSkillCharm( t, n, initm, charms ) )
			//	return;
		}
	}
}

void FindTwoSkillCharms( List_t< unsigned >^ charms, const unsigned index1, const unsigned index2, const unsigned max_charm_type )
{
	if( !StaticData::skill1_table || !StaticData::skill2_table )
		return;

	for( unsigned charm_type = 1; charm_type <= max_charm_type; ++charm_type )
	{
		int skill1_table_index;
		for( skill1_table_index = 0; skill1_table_index < StaticData::skill1_table[ charm_type ]->Length; skill1_table_index += 3 )
		{
			if( StaticData::skill1_table[ charm_type ][ skill1_table_index ] == index1 )
				break;
		}
		if( skill1_table_index >= StaticData::skill1_table[ charm_type ]->Length )
			continue;

		int skill2_table_index = -1;
		for( skill2_table_index = 0; skill2_table_index < StaticData::skill2_table[ charm_type ]->Length; skill2_table_index += 3 )
		{
			if( StaticData::skill2_table[ charm_type ][ skill2_table_index ] == index2 )
				break;
		}
		if( skill2_table_index >= StaticData::skill2_table[ charm_type ]->Length )
			continue;

		const unsigned num_skills1 = StaticData::skill1_table[ charm_type ]->Length / 3;
		const unsigned num_skills2 = StaticData::skill2_table[ charm_type ]->Length / 3;

		FindTwoSkillCharms( charms, skill1_table_index / 3, skill2_table_index / 3, num_skills1, num_skills2, charm_type );
	}
}

bool ContainsBetterCharm( List_t< Charm^ >^ charms, const int p1, const int p2, Ability^ ab1, Ability^ ab2 )
{
	for( int i = 0; i < charms->Count; ++i )
	{
		if( charms[i]->abilities[0]->ability == ab1 &&
			charms[i]->abilities[1]->ability == ab2 &&
			charms[i]->abilities[0]->amount >= p1 &&
			charms[i]->abilities[1]->amount >= p2 )
			return true;
	}
	return false;
}

bool ContainsBetterCharm( List_t< Charm^ >^ charms, Charm^ charm )
{
	for each( Charm^ c in charms )
	{
		if( c->abilities[0]->ability == charm->abilities[0]->ability &&
			c->abilities[0]->amount >= charm->abilities[0]->amount )
			return true;
		if( c->abilities.Count == 2 &&
			c->abilities[1]->ability == charm->abilities[0]->ability &&
			c->abilities[1]->amount >= charm->abilities[0]->amount )
			return true;
	}
	return false;
}

void GetDoubleSkillCharms( List_t< Charm^ >^ list, List_t< Skill^ >% skills, const unsigned max_charm_type )
{
	List_t< Charm^ > two_skills;

	for( int i = 1; i < skills.Count; ++i )
	{
		Ability^ ab1 = skills[ i ]->ability;
		for( int j = 0; j < i; ++j )
		{
			Ability^ ab2 = skills[ j ]->ability;

			List_t< unsigned > charms;

			FindTwoSkillCharms( %charms, ab1->static_index, ab2->static_index, max_charm_type );

			for( unsigned l = charms.Count; l --> 0; )
			{
				const int p1 = charms[l] >> 16;
				const int p2 = charms[l] & 0xFFFF;
				if( !ContainsBetterCharm( %two_skills, p1, p2, ab1, ab2 ) )
				{
					Charm^ c = gcnew Charm( 0 );
					c->abilities.Add( gcnew AbilityPair( ab1, p1 ) );
					c->abilities.Add( gcnew AbilityPair( ab2, p2 ) );

					two_skills.Add( c );
				}
			}	
			
			charms.Clear();

			FindTwoSkillCharms( %charms, ab2->static_index, ab1->static_index, max_charm_type );

			for( unsigned l = charms.Count; l --> 0; )
			{
				const int p1 = charms[l] >> 16;
				const int p2 = charms[l] & 0xFFFF;
				if( !ContainsBetterCharm( %two_skills, p1, p2, ab2, ab1 ) )
				{
					Charm^ c = gcnew Charm( 0 );
					c->abilities.Add( gcnew AbilityPair( ab2, p1 ) );
					c->abilities.Add( gcnew AbilityPair( ab1, p2 ) );

					two_skills.Add( c );
				}
			}
		}
	}

	for each( Charm^ c in list )
	{
		if( !ContainsBetterCharm( %two_skills, c ) )
			two_skills.Add( c );
	}

	list->Clear();
	list->AddRange( %two_skills );
}

void GetSingleSkillCharms( List_t< Charm^ >^ list, List_t< Skill^ >% skills, const unsigned max_charm_type )
{
	for each( Skill^ skill in skills )
	{
		if( skill->ability->max_vals1 == nullptr )
			continue;

		Charm^ ct = gcnew Charm;
		ct->num_slots = 0;
		ct->abilities.Add( gcnew AbilityPair( skill->ability, skill->ability->max_vals1[ max_charm_type ] ) );
		list->Add( ct );
	}
}

List_t< Charm^ >^ CharmDatabase::GetCharms( Query^ query, const bool use_two_skill_charms )
{
	List_t< Charm^ >^ res = gcnew List_t< Charm^ >;
	const unsigned max_charm_type = CalcMaxCharmType( query );

	GetSingleSkillCharms( res, query->skills, max_charm_type );

	if( use_two_skill_charms && max_charm_type > 0 )
		GetDoubleSkillCharms( res, query->skills, max_charm_type );

	return res;
}

CharmLocationDatum^ CharmDatabase::FindCharmLocations( Charm^ charm )
{
	CharmLocationDatum^ result = gcnew CharmLocationDatum();
	result->table = gcnew array< unsigned, 2 >( 17, NumCharmTypes );
	result->examples = gcnew array< System::String^ >( 17 );
	const unsigned limit = 64000;

	unsigned num_found = 0;
	for( unsigned charm_type = 0; charm_type < NumCharmTypes; ++charm_type )
	{
		if( charm->num_slots > charm_type + 1 )
			continue;

		if( charm->abilities.Count == 0 )
		{
			for( int t = 0; t < table_seeds->Length; ++t )
				result->table[t, charm_type] = limit;
			continue;
		}

		int skill1_table_index = -1;
		for( skill1_table_index = 0; skill1_table_index < StaticData::skill1_table[ charm_type ]->Length; skill1_table_index += 3 )
		{
			if( StaticData::skill1_table[ charm_type ][ skill1_table_index ] == charm->abilities[0]->ability->static_index &&
				StaticData::skill1_table[ charm_type ][ skill1_table_index + 2 ] >= charm->abilities[0]->amount )
				break;
		}

		const unsigned num1 = StaticData::skill1_table[ charm_type ]->Length / 3;

		if( charm->abilities.Count == 1 )
		{
			int skill2_table_index = -1;
			if( StaticData::skill2_table[ charm_type ] )
			{
				for( skill2_table_index = 0; skill2_table_index < StaticData::skill2_table[ charm_type ]->Length; skill2_table_index += 3 )
				{
					if( StaticData::skill2_table[ charm_type ][ skill2_table_index ] == charm->abilities[0]->ability->static_index &&
						StaticData::skill2_table[ charm_type ][ skill2_table_index + 2 ] >= charm->abilities[0]->amount )
						break;
				}
			}
			if( skill1_table_index >= StaticData::skill1_table[ charm_type ]->Length &&
				( skill2_table_index < 0 || skill2_table_index >= StaticData::skill2_table[ charm_type ]->Length ) )
				continue;

			for( int t = 0; t < table_seeds->Length; ++t )
			{
				if( t == 10 || t == 11  || t == 14 || t == 15 || t == 16 )
				{
					//do nothing. see MH4 implementation
				}
				else
				{
					result->table[t, charm_type] = limit;
				}
			}
		}
		else if( StaticData::skill2_table[ charm_type ] )
		{
			if( skill1_table_index >= StaticData::skill1_table[ charm_type ]->Length )
				continue;

			int skill2_table_index = -1;
			for( skill2_table_index = 0; skill2_table_index < StaticData::skill2_table[ charm_type ]->Length; skill2_table_index += 3 )
			{
				if( StaticData::skill2_table[ charm_type ][ skill2_table_index ] == charm->abilities[1]->ability->static_index &&
					StaticData::skill2_table[ charm_type ][ skill2_table_index + 2 ] >= charm->abilities[1]->amount )
					break;
			}
			if( skill2_table_index >= StaticData::skill2_table[ charm_type ]->Length )
				continue;

			const unsigned num2 = StaticData::skill2_table[ charm_type ]->Length / 3;

			for( int n = skill1_table_index / 3; n < 65363; n += num1 )
			{
				const int table = FindTable( n );
				if( table == -1 )
					continue;

				for( int m = skill2_table_index / 3; m < 65363; m += num2 )
				{
					const int initm = reverse_rnd(reverse_rnd(m == 0 ? num2 : m));
					String^ str = CanGenerateCharm2( charm_type, n, initm, charm );
					if( str )
					{
						if( result->table[table, charm_type]++ == 0 )
							result->examples[ table ] = str;
						else
							result->examples[ table ] = nullptr;

						if( num_found++ == 0 )
							result->example = str;
						else result->example = nullptr;
					}
				}
			}
		}
	}
	return result;
}
