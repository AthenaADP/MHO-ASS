#include "stdafx.h"
#include <fstream>
#include "Skill.h"
#include "Solution.h"
#include "LoadedData.h"

using namespace System;

int CompareAbilities( Ability^ a, Ability^ b )
{
	return String::Compare( a->name, b->name );
}

int CompareSkills( Skill^ a, Skill^ b )
{
	return String::Compare( a->name, b->name );
}

Skill^ Ability::GetSkill( const int amount )
{
	if( amount == 0 )
		return nullptr;

	int best = 0;
	SkillMap_t::Enumerator iter = skills.GetEnumerator();
	if( amount > 0 )
	{
		while( iter.MoveNext() )
			if( iter.Current.Key <= amount && iter.Current.Key > best )
				best = iter.Current.Key;
	}
	else
	{
		while( iter.MoveNext() )
			if( iter.Current.Key >= amount && iter.Current.Key < best )
				best = iter.Current.Key;
	}
	if( best == 0 ) return nullptr;
	Assert( skills.ContainsKey( best ), L"Skill dictionary doesn't contain this skill?" );
	return skills[ best ];
}

Skill^ Ability::GetWorstGoodSkill()
{
	Skill^ best = nullptr;
	SkillMap_t::Enumerator iter = skills.GetEnumerator();
	while( iter.MoveNext() )
	{
		if( iter.Current.Key <= 0 )
			continue;

		if( !best || iter.Current.Key < best->points_required )
			best = iter.Current.Value;
	}
	return best;
}

Ability^ Ability::FindAbility( System::String^ name )
{
	if( name && static_ability_map.ContainsKey( name ) )
		return static_ability_map[ name ];
	return nullptr;
}

Ability^ Ability::FindCharmAbility( System::String^ name )
{
	if( name && charm_ability_map.ContainsKey( name ) )
		return charm_ability_map[ name ];
	return nullptr;
}

void Ability::UpdateOrdering()
{
	ordered_abilities.Clear();
	ordered_abilities.AddRange( %static_abilities );
	ordered_abilities.Sort( gcnew Comparison< Ability^ >( CompareAbilities ) );
	for( int i = 0; i < ordered_abilities.Count; ++i )
		ordered_abilities[ i ]->order = i;
}

bool ContainsString( List_t< String^ >% vec, String^ item )
{
	for( int i = 0; i < vec.Count; ++i )
		if( vec[ i ] == item ) return true;
	return false;
}

int FindSkillIndex( Skill^ skill, List_t< Skill^ >^ vec )
{
	for( int i = 0; i < vec->Count; ++i )
		if( vec[ i ] == skill )
			return i;
	return -1;
}

void Skill::LoadSaveData( System::String^ filename )
{
	if( !IO::File::Exists( filename ) )
		return;

	Ability::savedata_abilities.Clear();
	Ability::savedata_abilities.Add( nullptr );

	IO::StreamReader fin( filename );

	unsigned line_no = 1;
	String^ temp;
	while( !fin.EndOfStream && temp != L"" )
	{
		temp = fin.ReadLine();

		if( temp == L"" )
			break;
		else if( temp[ 0 ] == L'#' )
			continue;

		Ability^ ab = Ability::FindCharmAbility( temp );

		Ability::savedata_abilities.Add( ab );
	}
}

void Skill::LoadCompound( String^ filename )
{
	if( !IO::File::Exists( filename ) )
		return;

	IO::StreamReader fin( filename );

	String^ temp;
	while( !fin.EndOfStream && temp != L"" )
	{
		temp = fin.ReadLine();
		
		if( temp == L"" )
			break;
		else if( temp[ 0 ] == L'#' )
			continue;

		List_t< String^ > split;
		Utility::SplitString( %split, temp, L',' );

		if( split.Count < 2 )
			continue;

		Skill^ compound = FindSkill( split[0] );
		Assert( compound, "No such compound skill found: " + split[0] );
		Assert( !compound_skill_map.ContainsKey( compound ), "Compound skill listed twice" );

		array< Skill^ >^ a = gcnew array< Skill^ >( split.Count - 1 );

		for( int i = 1; i < split.Count; ++i )
		{
			Skill^ subskill = FindSkill( split[i] );
			Assert( subskill, "No such compound sub skill found: " + split[i] );
			a[ i - 1 ] = subskill;
		}

		compound_skill_map.Add( compound, a );
	}
}

void Skill::Load( String^ filename )
{
	IO::StreamReader fin( filename );

	Ability::static_abilities.Clear();
	Ability::static_ability_map.Clear();
	static_skills.Clear();
	static_skill_map.Clear();
	ordered_skills.Clear();
	Ability::static_abilities.Capacity = 128;
	static_skills.Capacity = 256;

	//skill,ability,points,type012,tags...
	String^ temp;
	while( !fin.EndOfStream && temp != L"" )
	{
		temp = fin.ReadLine();
		if( temp == L"" ) break;
		else if( temp[ 0 ] == L'#' ) continue;
		List_t< String^ > split;
		Utility::SplitString( %split, temp, L',' );
		Skill^ skill = gcnew Skill;
		skill->best = false;
		//skill->ping_index = Convert::ToUInt32( split[ 0 ] );
		skill->name = split[ 0 ];
		/*if( split[ 1 ] == L"" )
		{
			Assert( !SpecificAbility::torso_inc, L"Multiple Torso Inc skills in data file" );
			SpecificAbility::torso_inc = gcnew Ability;
			SpecificAbility::torso_inc->efficient = false;
			SpecificAbility::torso_inc->has_decorations = false;
			SpecificAbility::torso_inc->has_1slot_decorations = false;
			SpecificAbility::torso_inc->name = split[ 0 ];
			SpecificAbility::torso_inc->jap_name = SpecificAbility::torso_inc->name;
			SpecificAbility::torso_inc->ping_index = 1;
			SpecificAbility::torso_inc->num_good_skills = 0;
			SpecificAbility::torso_inc->static_index = Ability::static_abilities.Count;
			Ability::static_abilities.Add( SpecificAbility::torso_inc );
			Ability::static_ability_map[ SpecificAbility::torso_inc->name ] = SpecificAbility::torso_inc;
			continue;
		}*/
		skill->points_required = Convert::ToInt32( split[ 2 ] );
		skill->ability = Ability::FindAbility( split[ 1 ] );
		if( !skill->ability )
		{
			Ability^ ability = gcnew Ability;
			ability->efficient = false;
			//ability->ping_index = Convert::ToUInt32( split[ 1 ] );
			ability->name = split[ 1 ];
			ability->chn_name = ability->name;
			ability->has_decorations = false;
			ability->has_1slot_decorations = false;

			ability->static_index = Ability::static_abilities.Count;
			ability->order = Ability::static_abilities.Count;
			ability->num_good_skills = 0;
			Ability::static_abilities.Add( ability );
			Ability::static_ability_map[ ability->name ] = ability;
			Ability::charm_ability_map[ ability->name ] = ability;
			skill->ability = ability;
			for( int i = 4; i < split.Count; ++i )
			{
				if( split[ i ] != L"" )
				{
					SkillTag^ tag = SkillTag::FindTag( split[ i ] );
					if( !tag )
					{
						throw gcnew Exception( L"Skill Tag '" + split[ i ] + L"' does not exist" );
					}
					ability->tags.Add( tag );
				}
			}
		}

		if( skill->ability == SpecificAbility::sense && skill->points_required == -10 )
			skill->is_taunt = true;

		skill->ability->skills[ skill->points_required ] = skill;
		if( skill->points_required < 0 )
			skill->ability->has_bad = true;
		else
			skill->ability->num_good_skills++;
		skill->static_index = static_skills.Count;
		skill->order = static_skills.Count;
		static_skills.Add( skill );
		static_skill_map[ skill->name ] = skill;
	}
	
	fin.Close();
	static_skills.TrimExcess();
	Ability::static_abilities.TrimExcess();
	Ability::UpdateOrdering();
	Skill::UpdateOrdering();

	for each( Ability^ a in Ability::static_abilities )
	{
		Skill^ s = a->GetSkill( 1000 );
		if( s )
			s->best = true;
	}

	SpecificAbility::defence = Ability::FindAbility( L"防御" );
	SpecificAbility::fire_res = Ability::FindAbility( L"火耐性" );
	SpecificAbility::water_res = Ability::FindAbility( L"水耐性" );
	SpecificAbility::thunder_res = Ability::FindAbility( L"雷耐性" );
	SpecificAbility::ice_res = Ability::FindAbility( L"氷耐性" );
	SpecificAbility::dragon_res = Ability::FindAbility( L"龍耐性" );
	SpecificAbility::sense = Ability::FindAbility( L"気配" );
	SpecificAbility::gathering = Ability::FindAbility( L"採取" );
}

Skill^ Skill::FindSkill( System::String^ name )
{
	if( static_skill_map.ContainsKey( name ) )
		return static_skill_map[ name ];
	else return nullptr;
}

void Skill::LoadLanguage( System::String^ filename )
{
	Ability::static_ability_map.Clear();
	IO::StreamReader fin( filename );

	for( int i = 0; i < Ability::static_abilities.Count; )
	{
		String^ line = fin.ReadLine();
		if( !line )
		{
			Windows::Forms::MessageBox::Show( L"Unexpected end of file: not enough lines of text in skill translations" );
			return;
		}

		if( line == L"" || line[ 0 ] == L'#' )
			continue;
		
		Ability::static_abilities[ i ]->name = line;
		Ability::static_ability_map.Add( line, Ability::static_abilities[ i ] );
		
		i++;
	}

	static_skill_map.Clear();
	for( int i = 0; i < static_skills.Count; )
	{
		String^ line = fin.ReadLine();
		if( !line )
		{
			Windows::Forms::MessageBox::Show( L"Unexpected end of file: not enough lines of text in skill translations" );
			return;
		}
		if( line == L"" || line[ 0 ] == L'#' )
			continue;

		static_skills[ i ]->name = line;
		static_skill_map.Add( line, static_skills[ i ] );
		
		i++;
	}
}

void Skill::LoadDescriptions( System::String^ filename )
{
	IO::StreamReader fin( filename );
	//SpecificAbility::torso_inc_desc = fin.ReadLine();
	for( int i = 0; i < Skill::static_skills.Count; ++i )
	{
		Skill::static_skills[ i ]->description = fin.ReadLine();
	}
}

void Skill::UpdateOrdering()
{
	ordered_skills.Clear();
	ordered_skills.AddRange( %static_skills );
	ordered_skills.Sort( gcnew Comparison< Skill^ >( CompareSkills ) );
	for( int i = 0; i < ordered_skills.Count; ++i )
		ordered_skills[ i ]->order = i;
}

bool Skill::CompoundSkillOverrides( Skill^ compound, Skill^ other )
{
	if( !compound_skill_map.ContainsKey( compound ) )
		return false;
	
	array< Skill^ >^ subskills = compound_skill_map[ compound ];
	for each( Skill^ s in subskills )
	{
		if( s->ability == other->ability && s->points_required >= other->points_required )
			return true;
	}
	return false;
}

void FindRelatedSkills( List_t< System::Windows::Forms::ComboBox^ >% skills, List_t< System::Windows::Forms::ComboBox^ >% skill_filters, List_t< Map_t< unsigned, unsigned >^ >% index_maps )
{
	for each( Ability^ a in Ability::static_abilities )
	{
		a->related = false;
	}

	List_t< Ability^ > selected_abilities;
	array< unsigned >^ relation_count = gcnew array< unsigned >( Ability::static_abilities.Count );
	for( int i = 0; i < skills.Count; ++i )
	{
		if( skills[ i ]->SelectedIndex <= 0 ||
			skill_filters[ i ]->SelectedIndex == 2 )
			continue;

		Ability^ a = Skill::static_skills[ index_maps[ i ][ skills[ i ]->SelectedIndex ] ]->ability ;
		relation_count[ a->static_index ] = 100; //lots because selected skills are related by definition
		selected_abilities.Add( a );
	}
	
	for each( List_t< Armor^ >^ la in Armor::static_armors )
	{
		for each( Armor^ a in la )
		{
			if( a->ContainsAnyAbility( selected_abilities ) )
			{
				for each( AbilityPair^ ap in a->abilities )
				{
					if( ap->amount > 0 )
						relation_count[ ap->ability->static_index ]++;
				}
			}
		}
	}
	
	for( int i = 0; i < relation_count->Length; ++i )
	{
		if( relation_count[ i ] > 4 )
			Ability::static_abilities[ i ]->related = true;
	}
}

int CompareAbilitiesByName( Ability^ a1, Ability^ a2 )
{
	return System::String::Compare( a1->name, a2->name );
}
