#include "stdafx.h"
#include "Armor.h"
#include "Skill.h"
#include "Solution.h"
#include <fstream>

using namespace System;

String^ GetFirst( String^ s, int len )
{
	return s->Length < len ? s : s->Substring( 0, len );
}

String^ GetLast( String^ s, int len )
{
	return s->Length < len ? s : s->Substring( s->Length - len, len );
}

bool ArmorsHaveSameComponents( Armor^ a, Armor^ b )
{
	if( a->components.Count != b->components.Count )
		return false;

	for( int i = 0; i < a->components.Count; ++i )
	{
		if( a->components[ i ]->amount != b->components[ i ]->amount ||
			a->components[ i ]->material != b->components[ i ]->material )
			return false;
	}
	return true;
}

unsigned GetFamily( Armor^ armor )
{
	String^ tag = GetFirst( armor->name, 3 );
	//if( tag == L"武装へ" )
	//	tag = L"モンハ";

	/*if( armor->name->StartsWith( L"EX" ) )
		tag = GetFirst( armor->name, 4 );
	else
	if( armor->name->StartsWith( L"ゴア" ) ||
		armor->name->StartsWith( L"宝纏" ) ||
		armor->name->StartsWith( L"岩穿" ) ||
		armor->name->StartsWith( L"荒天" ) ||
		armor->name->StartsWith( L"蒼天" ) ||
		armor->name->StartsWith( L"矛砕" ) ||
		armor->name->StartsWith( L"紅兜" ) ||
		armor->name->StartsWith( L"異形" ) ||
		armor->name->StartsWith( L"隻眼" ) ||
		armor->name->StartsWith( L"モス" ) )
		tag = GetFirst( armor->name, 2 );
	else 
	if( armor->name->StartsWith( L"忍の" ) )
		tag = GetLast( armor->name, 2 );*/

	if( Family::map.ContainsKey( tag ) )
	{
		const unsigned res = Family::map[ tag ];
		Family::families[ res ]->Add( armor );
		return res;
	}
	Family::map.Add( tag, Family::count );
	List_t< Armor^ >^ list = gcnew List_t< Armor^ >;
	list->Add( armor );
	Family::families.Add( list );
	return Family::count++;
}

void Armor::Load( String^ filename, ArmorType armor_type )
{
	List_t< Armor^ >^ armors = static_armors[ (int)armor_type ];
	armors->Clear();
	armors->Capacity = 512;

	unsigned num_male = 0, num_female = 0;
	
	IO::StreamReader fin( filename );
	String^ temp;
	while( !fin.EndOfStream && temp != L"" )
	{
		temp = fin.ReadLine();
		if( temp == L"" ) break;
		else if( temp[ 0 ] == L'#' ) continue;
		List_t< String^ > split;
		Utility::SplitString( %split, temp, L',' );
		
		Armor^ armor = gcnew Armor();
		armor->torso_inc = false;
		armor->danger = nullptr;
		//Name,gender012,type012,level,hr,rarity,slots,def,max def,fire,water,ice,lightning,dragon,skill1,##,skill2,##,skill3,##,skill4,##,skill5,##,skill6,##,skill7,##,mat1,##,mat2,##,mat3,##,mat4,##,mat5,##
		//0   ,1        ,2      ,3    ,4 ,5     ,6    ,7  ,8      ,9   ,10   ,11 ,12       ,13    ,14    ,15,16    ,17,18    ,19,20    ,21,22    ,23,24    ,25,26    ,27,28  ,29,30  ,31,32  ,33,34  ,35,36  ,37
		armor->name = split[ 0 ];
		armor->chn_name = split[ 0 ];
		armor->gender = split[ 1 ] == L"1" ? Gender::MALE : split[ 1 ] == L"2" ? Gender::FEMALE : Gender::BOTH_GENDERS;
		armor->type = split[ 2 ] == L"0" ? HunterType::BLADEMASTER : HunterType::GUNNER;
		armor->rarity = Convert::ToInt32( split[ 5 ] );

		if( split[ 6 ] == L"" )
			armor->num_slots = 0;
		else
			armor->num_slots = Convert::ToUInt32( split[ 6 ] );

		armor->hr = Convert::ToUInt32( split[ 4 ] );
		armor->level = Convert::ToUInt32( split[ 3 ] );

		armor->defence = Convert::ToUInt32( split[ 7 ] );
		armor->max_defence = Convert::ToUInt32( split[ 8 ] );
		armor->fire_res = Convert::ToInt32( split[ 9 ] );
		armor->water_res = Convert::ToInt32( split[ 10 ] );
		armor->thunder_res = Convert::ToInt32( split[ 11 ] );
		armor->ice_res = Convert::ToInt32( split[ 12 ] );
		armor->dragon_res = Convert::ToInt32( split[ 13 ] );
		armor->difficulty = 0;
		const unsigned material_start = 28;
		for( unsigned i = 0; i < 5; ++i )
		{
			String^% name = split[ i * 2 + material_start ];

			if( name != L"" && !name->StartsWith( L"※" ) )
			{
				if( name->EndsWith( L"pts)" ) )
					name = name->Substring( 0, name->LastIndexOf( "(" ) );

				MaterialComponent^ comp = gcnew MaterialComponent;
				comp->material = Material::FindMaterial( name );
				if( comp->material )
				{
					comp->amount = Convert::ToInt32( split[ i * 2 + material_start + 1 ] );
					armor->components.Add( comp );

					armor->arena |= comp->material->arena;
					armor->difficulty += comp->material->difficulty * comp->amount;
				}
			}
		}
		
		const unsigned skill_start = 14;
		for( unsigned i = 0; i < 7; ++i )
		{
			const unsigned index = skill_start + i * 2;
			if( split[ index ] != L"" )
			{
				AbilityPair^ p = gcnew AbilityPair( Ability::FindAbility( split[ index ] ), 0 );
				Assert( p->ability, L"No such ability found: " + split[ index ] );
				if( split[ index + 1 ] != L"" )
					p->amount = Convert::ToInt32( split[ index + 1 ] );
				armor->abilities.Add( p );
				if( p->ability == SpecificAbility::torso_inc )
					armor->torso_inc = true;
			}
		}
		
		if( armor->gender == Gender::MALE )
		{
			armor->male_image = ++num_male;
			armor->female_image = 0;
		}
		else if( armor->gender == Gender::FEMALE )
		{
			armor->female_image = ++num_female;
			armor->male_image = 0;
		}
		else
		{
			armor->male_image = ++num_male;
			armor->female_image = ++num_female;
		}

		armor->chakra = armor->abilities.Count == 0 && armor->num_slots == 3;

		armor->gunner_def = armors->Count > 0 && armor->max_defence < armors[ armors->Count - 1 ]->max_defence && ArmorsHaveSameComponents( armor, armors[ armors->Count - 1 ] );
			
		armor->family = GetFamily( armor );
		armor->index = armors->Count;
		armors->Add( armor );
	}

	fin.Close();
	armors->TrimExcess();
}

unsigned GetTier( Armor^ a )
{
	return a->rarity / 4;
}

unsigned GetTier( const unsigned hr, const unsigned level )
{
	return hr > 0 ? 1 : 0;
}

bool Armor::MatchesQuery( Query^ query, List_t< Ability^ >^ danger_skills, const unsigned max_slots )
{
	//check requirements
	if( !query->allow_lower_tier && GetTier( this ) < GetTier( query->hr, this->level ) ||
		type != HunterType::BOTH_TYPES && query->hunter_type != type ||
		gender != Gender::BOTH_GENDERS && gender != query->gender ||
		!query->HRMatches( hr, level ) )
		return false;
	//check for torso inc / chakra gear
	if( torso_inc || chakra )
		return true;
	//check for danger skills
	danger = nullptr;
	for each( AbilityPair^ apair in abilities )
	{
		if( apair->amount < 0 && Utility::Contains( danger_skills, apair->ability ) )
		{
			danger = apair->ability;
			break;
		}
	}
	//check for relevant skills
	no_skills = true;
	bool bad_skills = false;
	for( int i = 0; i < abilities.Count; ++i )
	{
		if( Utility::Contains( %query->rel_abilities, abilities[ i ]->ability ) )
		{
			if( abilities[ i ]->amount > 0 )
			{
				no_skills = false;
				return true;
			}
			else bad_skills = true;
		}
		else if( query->want_taunt && abilities[ i ]->ability == SpecificAbility::sense && abilities[ i ]->amount < 0 )
		{
			no_skills = false;
			return true;
		}
	}
	if( num_slots == max_slots )
		return !danger && !bad_skills;
	else
		return num_slots > max_slots && !danger;
}

bool Armor::IsBetterAtNonSkills( Armor^ other )
{
	return this->defence != other->defence ? this->defence > other->defence :
		this->max_defence != other->max_defence ? this->max_defence > other->max_defence :
		this->fire_res != other->fire_res ? this->fire_res > other->fire_res :
		this->dragon_res != other->dragon_res ? this->dragon_res > other->dragon_res :
		this->ice_res != other->ice_res ? this->ice_res > other->ice_res :
		this->thunder_res != other->thunder_res ? this->thunder_res > other->thunder_res :
		this->water_res != other->water_res ? this->water_res > other->water_res :
		this->difficulty < other->difficulty;
}

bool Armor::IsBetterThan( Armor^ other, List_t< Ability^ >^ rel_abilities, const bool want_taunt )
{
	if( num_slots > other->num_slots ||
		num_slots == other->num_slots && !no_skills && other->no_skills )
		return true;

	if( this->no_skills && other->no_skills ||
		this->torso_inc && other->torso_inc )
	{
		return this->IsBetterAtNonSkills( other );
	}
	else if( this->torso_inc || other->torso_inc )
		return true;

	bool somewhat_worse = num_slots < other->num_slots;
	int total_points = 0, other_total_points = 0;

	for each( Ability^ ability in rel_abilities )
	{
		int my_skill_at = this->GetSkillAt( ability );
		int other_skill_at = other->GetSkillAt( ability );

		total_points += my_skill_at;
		other_total_points += other_skill_at;

		if( ability->efficient )
		{
			my_skill_at += num_slots * 2;
			other_skill_at += other->num_slots * 2;
		}

		if( my_skill_at > other_skill_at )
			return true;
		else if( my_skill_at < other_skill_at )
			somewhat_worse = true;
	}

	if( want_taunt )
	{
		const int my_taunt = this->GetSkillAt( SpecificAbility::sense );
		const int ot_taunt = other->GetSkillAt( SpecificAbility::sense );
		if( my_taunt < ot_taunt )
			return true;
		else if( my_taunt > ot_taunt )
			somewhat_worse = true;
	}

	return total_points > other_total_points || ( !somewhat_worse && this->IsBetterAtNonSkills( other ) );
}

int Armor::GetSkillAt( Ability^ ability )
{
	for each( AbilityPair^ apair in abilities )
	{
		if( apair->ability == ability )
			return apair->amount;
	}
	return 0;
}

void Armor::LoadLanguage( String^ filename, ArmorType armor_type )
{
	List_t< Armor^ >^ armors = static_armors[ (int)armor_type ];
	IO::StreamReader fin( filename );
	for( int i = 0; i < armors->Count; ++i )
	{
		Armor^ armor = armors[ i ];
		String^ line = fin.ReadLine();
		if( !line )
		{
			array< wchar_t >^ slashes = { L'/', L'\\' };
			const int slash = filename->LastIndexOfAny( slashes );
			Windows::Forms::MessageBox::Show( L"Unexpected end of file: not enough lines of text?", filename->Substring( slash + 1 ) );
			break;
		}

		if( line == L"" || line[ 0 ] == L'#' )
			continue;

		armor->name = line;

		if( !StringTable::chinese )
			armor->eng_name = line;
	}
}

bool Armor::ContainsAnyAbility( List_t< Ability^ >% to_search )
{
	for each( AbilityPair^ ap in abilities )
	{
		if( ap->amount > 0 && Utility::Contains( %to_search, ap->ability ) )
			return true;
	}
	return false;
}

void Armor::SetExName( const int type )
{
	array< String^ >^ names =
	{
		StaticString( Helm ),
		StaticString( Torso ),
		StaticString( Arms ),
		StaticString( Waist ),
		StaticString( Legs ),
	};
	name = names[ type ] + L": +" + Convert::ToString( abilities[ 0 ]->amount ) + L" " + abilities[ 0 ]->ability->name;
}

String^ Charm::GetName()
{
	System::Text::StringBuilder sb;
	if( abilities.Count > 0 )
	{
		sb.Append( L"+" )->Append( abilities[ 0 ]->amount )->Append( L" " )->Append( abilities[ 0 ]->ability->name );
		for( int i = 1; i < abilities.Count; ++i )
			sb.Append( abilities[ i ]->amount > 0 ? L", +" : L", " )->Append( abilities[ i ]->amount )->Append( L" " )->Append( abilities[ i ]->ability->name );
		
	}
	sb.Append( L" " )->Append( Utility::SlotString( num_slots ) );
	return sb.ToString();
}

Charm::Charm( Charm^ other ) : num_slots( other->num_slots ), custom( other->custom ), optimal( false )
{
	for each( AbilityPair^ ap in other->abilities )
	{
		abilities.Add( gcnew AbilityPair( ap->ability, ap->amount ) );
	}
}

Charm::Charm( const unsigned num_slots ) : num_slots( num_slots ), custom( false ), optimal( false ) {}

int Charm::GetSkillPointsFor( Ability^ a )
{
	if( abilities.Count > 0 )
	{
		if( abilities[0]->ability == a )
			return abilities[0]->amount;
		else if( abilities.Count > 1 && abilities[1]->ability == a )
			return abilities[1]->amount;
	}
	return 0;
}

bool Charm::StrictlyBetterThan( Charm^ other )
{
	if( num_slots < other->num_slots )
		return false;

	bool better = false;

	List_t< Ability^ > rel_abilities;
	for each( AbilityPair^ apair in other->abilities )
	{
		if( apair->ability->relevant && !Utility::Contains( %rel_abilities, apair->ability ) )
			rel_abilities.Add( apair->ability );
	}
	for each( AbilityPair^ apair in abilities )
	{
		if( apair->ability->relevant && !Utility::Contains( %rel_abilities, apair->ability ) )
			rel_abilities.Add( apair->ability );
	}

	unsigned extra_slots = num_slots - other->num_slots;

	for each( Ability^ a in rel_abilities )
	{
		const int other_amount = other->GetSkillPointsFor( a );
		const int my_amount = GetSkillPointsFor( a );

		if( my_amount > other_amount )
		{
			better = true;
		}
		else if( my_amount < other_amount )
		{
			if( a->efficient && extra_slots > 0 )
			{
				const int extra_worth = ( other_amount - my_amount + 1 ) / 2;
				if( extra_worth <= (int)extra_slots )
				{
					extra_slots -= extra_worth;
					if( my_amount + extra_worth * 2 > other_amount )
						better = true;
					continue;
				}
			}
			return false;
		}
	}
	return better || num_slots > other->num_slots;
}

bool EqualAbilities( List_t< AbilityPair^ >% l1, List_t< AbilityPair^ >% l2 )
{
	Assert( l1.Count == l2.Count, L"Ability lists not equal size" );
	for( int i = 0; i < l1.Count; ++i )
		if( l1[ i ]->ability != l2[ i ]->ability ||
			l1[ i ]->amount != l2[ i ]->amount )
			return false;
	return true;
}

bool Charm::operator==( Charm^ other )
{
	return num_slots == other->num_slots &&
		abilities.Count == other->abilities.Count &&
		EqualAbilities( abilities, other->abilities );
}

bool Charm::BasicallyTheSameAs( Charm^ other )
{
	if( num_slots != other->num_slots )
		return false;
	List_t< AbilityPair^ > a1, a2;
	for each( AbilityPair^ ap in abilities )
		if( ap->ability->relevant )
			a1.Add( ap );
	for each( AbilityPair^ ap in other->abilities )
		if( ap->ability->relevant )
			a2.Add( ap );
	return a1.Count == a2.Count && EqualAbilities( a1, a2 );
}

unsigned Charm::GetHash()
{
	if( abilities.Count == 0 )
		return Charm::HashFunction( num_slots, nullptr, 0, nullptr, 0 );
	else if( abilities.Count == 1 )
		return Charm::HashFunction( num_slots, abilities[ 0 ]->ability, abilities[ 0 ]->amount, nullptr, 0 );
	else
		return Charm::HashFunction( num_slots, abilities[ 0 ]->ability, abilities[ 0 ]->amount, abilities[ 1 ]->ability, abilities[ 1 ]->amount );
}

unsigned Charm::HashFunction( const unsigned num_slots, const int ab1_index, const int p1, const int ab2_index, const int p2 )
{
	//2 bits for slots
	//4 bits for point1
	//5 bits for point2
	//8 bits for ability1
	//8 bits for ability2
	return num_slots + ( ab1_index < Ability::static_abilities.Count ? p1 << 2 : 0  ) + ( ( ab2_index < Ability::static_abilities.Count ? p2 + 12 : 12 ) << 6 ) + ( ab1_index << 11 ) + ( ab2_index << 19 );
}

unsigned Charm::HashFunction( const unsigned num_slots, Ability^ a1, const int p1, Ability^ a2, const int p2 )
{
	Assert( Ability::static_abilities.Count < 256, L"Not enough bits to hash ability" );
	const unsigned ab1_index = a1 ? a1->static_index : Ability::static_abilities.Count;
	const unsigned ab2_index = a2 ? a2->static_index : Ability::static_abilities.Count;
	return HashFunction( num_slots, ab1_index, p1, ab2_index, p2 );
}

void Charm::AddToOptimalList( List_t< Charm^ >% lst, Charm^ new_charm )
{
	for( int i = 0; i < lst.Count; ++i )
	{
		Charm^ curr_charm = lst[ i ];
		if( new_charm->StrictlyBetterThan( curr_charm ) )
			lst.RemoveAt( i-- );
		else if( curr_charm->StrictlyBetterThan( new_charm ) ||
			curr_charm->BasicallyTheSameAs( new_charm ) )
			return;
	}
	lst.Add( new_charm );
}

#define TryCompare( X ) if( a->X > b->X ) return -1;\
						else if( b->X > a->X ) return 1

#define TryCompare2( X ) if( a->X > b->X ) return 1;\
						 else if( b->X > a->X ) return -1

int CompareCharms1( Charm^ a, Charm^ b )
{
	if( a->abilities.Count > 0 )
	{
		if( b->abilities.Count == 0 )
			return 1;
		TryCompare2( abilities[ 0 ]->ability->static_index );
		TryCompare( abilities[ 0 ]->amount );
		if( a->abilities.Count > 1 )
		{
			if( b->abilities.Count < 2 )
				return 1;
			TryCompare2( abilities[ 1 ]->ability->static_index );
			TryCompare( abilities[ 1 ]->amount );
		}
		else if( b->abilities.Count > 1 )
			return -1;
	}
	else if( b->abilities.Count > 0 )
		return -1;
	TryCompare( num_slots );
	return 0;
}

int CompareCharms1Alphabetically( Charm^ a, Charm^ b )
{
	if( a->abilities.Count > 0 )
	{
		if( b->abilities.Count == 0 )
			return 1;
		TryCompare2( abilities[ 0 ]->ability->order );
		TryCompare( abilities[ 0 ]->amount );
		if( a->abilities.Count > 1 )
		{
			if( b->abilities.Count < 2 )
				return 1;
			TryCompare2( abilities[ 1 ]->ability->order );
			TryCompare( abilities[ 1 ]->amount );
		}
		else if( b->abilities.Count > 1 )
			return -1;
	}
	else if( b->abilities.Count > 0 )
		return -1;
	TryCompare( num_slots );
	return 0;
}

int CompareCharms2( Charm^ a, Charm^ b )
{
	if( a->abilities.Count < 2 )
	{
		if( b->abilities.Count < 2 )
			return CompareCharms1( a, b );
		else
			return 1;
	}
	else
	{
		if( b->abilities.Count < 2 )
			return -1;
		else
		{
			TryCompare2( abilities[ 1 ]->ability->static_index );
			TryCompare( abilities[ 1 ]->amount );
			TryCompare( num_slots );
			return 0;
		}
	}
}

int CompareCharms2Alphabetically( Charm^ a, Charm^ b )
{
	if( a->abilities.Count < 2 )
	{
		if( b->abilities.Count < 2 )
			return CompareCharms1Alphabetically( a, b );
		else
			return 1;
	}
	else
	{
		if( b->abilities.Count < 2 )
			return -1;
		else
		{
			TryCompare2( abilities[ 1 ]->ability->order );
			TryCompare( abilities[ 1 ]->amount );
			TryCompare( num_slots );
			return 0;
		}
	}
}
#undef TryCompare
#undef TryCompare2
