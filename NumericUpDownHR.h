#pragma once
#include "Common.h"

namespace MHOASS 
{
	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;

	public ref class NumericUpDownLvl : public NumericUpDown
	{
	public:
		NumericUpDownLvl() {}

		virtual void UpdateEditText() override
		{
			if( !StringTable::text )
				Text = Value.ToString();
			else
			{
				switch ((int)Value)
				{
				case 1:
					Text = Convert::ToString( 1 ); break;
				case 2:
					Text = Convert::ToString( 10 ); break;
				case 3:
					Text = Convert::ToString( 20 ); break;
				case 4:
					Text = Convert::ToString( 30 ); break;
				case 5:
					Text = Convert::ToString( 40 ); break;
				default:
					Text = Value.ToString(); break;
				}
			}
		}
	};

	public ref class NumericUpDownHR : public NumericUpDown
	{
	public:
		NumericUpDownHR() {}
	
		virtual void UpdateEditText() override
		{
			if( !StringTable::text )
				Text = Value.ToString();
			else
			{
				switch ((int)Value)
				{
				case 1:
					Text = "Lv1"; break;
				case 2:
					Text = StaticString( HR12 ); break;
				case 3:
					Text = StaticString( HR34 ); break;
				case 4:
					Text = StaticString( HRAll ); break;
				default:
					Text = Value.ToString(); break;
				}
			}
		}
	};
}