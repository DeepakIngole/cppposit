/**
 * Emanuele Ruffaldi (C) 2017
 * Templated C++ Posit
 */
#pragma once

#include <cstdint>
#include <iostream>
#include <ratio>
#include <bitset>
#include <inttypes.h>
#include "bithippop.hpp"
#include <math.h>
#include "floattraits.hpp"
#include "typehelpers.hpp"

inline std::ostream & operator << (std::ostream & ons, __int128_t x)
{
    ons << "cannot print int128";
    return ons;
}

template<class T>
constexpr const T& clamp( const T& v, const T& lo, const T& hi )
{
    return v < lo ? lo : v > hi ? hi : v;
}


template <class T>
constexpr T FLOORDIV(T a, T b) {return    ((a) / (b) - ((a) % (b) < 0));}


template <class FT = uint64_t, class ET = int32_t>
struct Unpacked
{
    static_assert(std::is_unsigned<FT>::value,"Unpacked requires unsigned fractiont type");
    static_assert(std::is_signed<ET>::value,"Unpacked requires signed exponent type");
    using POSIT_LUTYPE = FT;
	enum {
		POSIT_FRAC_TYPE_SIZE_BITS = sizeof(FT) * 8
	};
	enum : FT {
		POSIT_FRAC_TYPE_MSB = (((FT)1) << (POSIT_FRAC_TYPE_SIZE_BITS - 1))
	};
    #ifndef UnpackedDualSel
    #define UnpackedDualSel(a,b) ((a)+(b)*4)
    #endif


	enum Type { Regular, Infinity, NaN, Zero }; /// signed infinity and nan require the extra X bit
	Type type = Regular;
	bool negativeSign = false;
	ET exponent = 0; // with sign
	FT fraction = 0; // this can be 52bit for holding double

    explicit constexpr Unpacked() {}

    // assume fraction is denormalized form
    constexpr const Unpacked & normalize()
    {
        if(fraction == 0)
        {
            negativeSign = false;
            type = Zero;
        }
        else
        {
            int k = findbitleftmostC(fraction);

            exponent -= k;
            fraction <<= k+1; // plus normalization
            type = Regular;
        }
        return *this;
    }

	explicit CONSTEXPR14 Unpacked(float p) { unpack_float(p); }
	explicit CONSTEXPR14 Unpacked(double p) { unpack_double(p); }
    explicit CONSTEXPR14 Unpacked(halffloat p) { unpack_half(p); }
    explicit CONSTEXPR14 Unpacked(Type t , bool anegativeSign = false): type(t) ,negativeSign(anegativeSign) {};

    // expect 1.xxxxxx otherwise make it 0.xxxxxxxxx
    explicit CONSTEXPR14 Unpacked(ET aexponent, FT afraction, bool anegativeSign ): type(Regular) ,negativeSign(anegativeSign),exponent(aexponent),fraction(afraction)  {}

	CONSTEXPR14 void unpack_float(float f) { unpack_xfloat<single_trait>(f); }
	CONSTEXPR14 void unpack_double(double d) { unpack_xfloat<double_trait>(d); }
    CONSTEXPR14 void unpack_half(halffloat d) { unpack_xfloat<half_trait>(d); }

	constexpr operator float () const { return  pack_xfloat<single_trait>(); }
	constexpr operator double () const { return  pack_xfloat<double_trait>(); }
    constexpr operator halffloat() const { return  pack_xfloat<half_trait>(); }

	template <class Trait>
	CONSTEXPR14 typename Trait::holder_t
	pack_xfloati() const;

    template <class Trait>
    typename Trait::value_t
    pack_xfloat() const 
    {
        union {
            typename Trait::holder_t i;
            typename Trait::value_t  f; 
        } uu;
        uu.i = pack_xfloati<Trait>();
        return uu.f;
    }

	template <class T>
	constexpr T pack_float() const { return pack_xfloat<typename float2trait<T>::trait > (); }


	constexpr bool isInfinity() const { return type == Infinity; }
	constexpr bool isRegular() const { return type == Regular; }
	constexpr bool isNaN() const { return type == NaN; }
	constexpr bool isZero() const { return type == Zero; }
	constexpr bool isPositive() const { return !negativeSign; }

    static constexpr Unpacked infinity() { return Unpacked(Infinity); }
    static constexpr Unpacked pinfinity() { return Unpacked(Infinity,false); }
    static constexpr Unpacked ninfinity() { return Unpacked(Infinity,true); }
    static constexpr Unpacked nan() { return Unpacked(NaN); }
    static constexpr Unpacked one() { return Unpacked(0,0,false); }
    static constexpr Unpacked zero() { return Unpacked(Zero);  }


	bool operator == (const Unpacked & u) const { return negativeSign == u.negativeSign && type == u.type && (type == Regular ?exponent == u.exponent && fraction == u.fraction : true); }
	bool operator != (const Unpacked & u) const { return !(*this == u); }
    constexpr Unpacked operator-() const { return Unpacked(exponent,fraction,!negativeSign);  }

    CONSTEXPR14 Unpacked inv() const
    {
        switch(type)
        {
            case Regular:
                if(fraction == 0)
                {
                   // std::cout << "[exponent inversion " <<  std::dec  << " exponent" << exponent <<  "] becomes " << -exponent << std::endl;
                    return Unpacked(-exponent,0,negativeSign);
                }
                else
                {

                	// one == 0,0,false
    	        // TODO FIX SIGN/INFINITY/NAN
    		        // put hidden 1. in mantiss
    			    POSIT_LUTYPE afrac = POSIT_FRAC_TYPE_MSB;
    			    POSIT_LUTYPE bfrac = POSIT_FRAC_TYPE_MSB | (fraction >> 1);
                 //   std::cout << "inversion " << std::hex  << bfrac << " exponent" << exponent << std::endl;
    			    auto exp =  - exponent;

    			    if (afrac < bfrac) {
    			        exp--;
    			        bfrac >>= 1;
    			    }

    			    return Unpacked(exp, ( ((typename nextinttype<FT>::type)afrac)  << POSIT_FRAC_TYPE_SIZE_BITS) / bfrac,negativeSign);

                    //return one()/(*this);
                }
                break;
            case Infinity:
                return zero();
            case Zero:
                return infinity();
            case NaN:
	default:
                return *this;
        }

    }
	

	template <class Trait>
	CONSTEXPR14 void unpack_xfloati(typename Trait::holder_t value);

    template <class Trait>
    void unpack_xfloat(typename Trait::value_t value) // CANNOT be constexpr
    {
        union {
            typename Trait::holder_t i;
            typename Trait::value_t  f; 
        } uu;
        uu.f = value;
        unpack_xfloati<Trait>(uu.i);
    }

    constexpr friend Unpacked operator - (Unpacked a, Unpacked b)
    {
        return a+(-b);
    }

    Unpacked& operator+=(const Unpacked &a) { Unpacked r = *this+a; *this = r; return *this; }

    CONSTEXPR14 friend Unpacked operator+ ( Unpacked  a,  Unpacked  b) 
    {
        if(a.isNaN() || b.isNaN())
            return a;
        switch(UnpackedDualSel(a.type,b.type))
        {
            case UnpackedDualSel(Regular,Regular):
            {
                auto dir = a.exponent-b.exponent;
                ET exp = (dir < 0 ? b.exponent: a.exponent)+1;

                // move right means increment exponent
                // 1.xxxx => 0.1xxxxxx 
                // 1.yyyy => 0.1yyyyyy
                POSIT_LUTYPE afrac1 = (POSIT_FRAC_TYPE_MSB>>1) | (a.fraction >> 2); // denormalized and shifted right
                POSIT_LUTYPE bfrac1 = (POSIT_FRAC_TYPE_MSB>>1) | (b.fraction >> 2);
                POSIT_LUTYPE afrac = dir < 0 ? (afrac1 >> -dir) : afrac1; // denormalized and shifted right
                POSIT_LUTYPE bfrac = dir < 0 ? bfrac : (bfrac1 >> dir); 
                
                #if 0                
                if(dir < 0) // output will be in the b exponent
                {
                    // add exponent, reduce fraction
                    afrac >>= -dir;
                }
                else
                {
                    // add exponent, reduce fraction
                    bfrac >>= dir;
                }
                exp++; // due to the spacing in fraction
                #endif

                // 1.xxxx => 0.1xxxxx => 0.0k 1 xxxx
                //
                // if dir==0 then: 
                //   0.1xxxxx
                //   0.1yyyyy
                //   1.zzzzzz
                //
                // but also
                //   0.1xxxx
                //   0.0001yyyy
                //   0.1zzzz
                //
                // if 1. we easily normalize by shift
                // if 0. we pre
                int mode = a.negativeSign == b.negativeSign ? 0 : afrac > bfrac ? 1 : -1;
                bool osign = mode >= 0 ? a.negativeSign : b.negativeSign;
                POSIT_LUTYPE frac = mode == 0 ? afrac+bfrac: mode > 0 ? afrac - bfrac : bfrac-afrac;

                #if 0
                // same is easy
                if(a.negativeSign == b.negativeSign)
                {
                    osign = a.negativeSign;
                    frac = afrac+bfrac; // this will overflow from 63bits to 64bits
                }
                /**
                 * a+ a>b  a-b output+
                 * a- a>b  a-b output-
                 * a+ a<b  b-a output-
                 * a- a<b  b-a output+
                 */
                else if(afrac > bfrac) {
                    osign = a.negativeSign;
                    frac = afrac - bfrac;
                }
                else
                {
                    osign = b.negativeSign;
                    frac = bfrac-afrac;
                }
                #endif
                return Unpacked(exp, frac, osign).normalize();  // pass denormalized
            }
            case UnpackedDualSel(Regular,Zero):
            case UnpackedDualSel(Zero,Zero):
            case UnpackedDualSel(Infinity,Zero):
                return a;
            case UnpackedDualSel(Zero,Regular):
            case UnpackedDualSel(Zero,Infinity):
                return b;
            default: // case UnpackedDualSel(Infinity,Infinity):
                return (a.negativeSign == b.negativeSign)? a : nan();
        }
    }

    // https://www.edwardrosten.com/code/fp_template.html
    // https://github.com/Melown/half

    CONSTEXPR14 friend Unpacked operator*(const Unpacked & a, const Unpacked & b) 
    {
        if(a.isNaN() || b.isNaN())
            return a;
        switch(UnpackedDualSel(a.type,b.type))
        {
            case UnpackedDualSel(Regular,Regular):
            {
                POSIT_LUTYPE afrac = POSIT_FRAC_TYPE_MSB | (a.fraction >> 1);
                POSIT_LUTYPE bfrac = POSIT_FRAC_TYPE_MSB | (b.fraction >> 1);
                auto frac = ((((typename nextinttype<FT>::type)afrac) * bfrac) >> POSIT_FRAC_TYPE_SIZE_BITS);
                bool q = (frac & POSIT_FRAC_TYPE_MSB) == 0;
                auto rfrac = q ? (frac << 1): frac;
                auto exp = a.exponent + b.exponent + (q ? 0: 1);
                #if 0
                if ((frac & POSIT_FRAC_TYPE_MSB) == 0) {
                    exp--;
                    frac <<= 1;
                }
                #endif
                return Unpacked(exp, rfrac << 1,a.negativeSign ^ b.negativeSign);
            }
            case UnpackedDualSel(Regular,Zero):
            case UnpackedDualSel(Zero,Regular):
            case UnpackedDualSel(Zero,Zero):
                return zero();
            case UnpackedDualSel(Infinity,Zero):
            case UnpackedDualSel(Zero,Infinity):
                return nan();
            default: // case UnpackedDualSel(Infinity,Infinity):
                // inf inf or inf reg or reg inf
                return (a.negativeSign^b.negativeSign)? ninfinity() : pinfinity();
        }
	}

    /**
     * Division Truth Table

     */
    CONSTEXPR14 friend Unpacked operator/ (const Unpacked & a, const Unpacked & b) 
    {
        if(a.isNaN() || b.isNaN())
            return a;

        // 9 more cases
        switch(UnpackedDualSel(a.type,b.type))
        {
            case UnpackedDualSel(Regular,Regular):
            {
                POSIT_LUTYPE afrac = POSIT_FRAC_TYPE_MSB | (a.fraction >> 1);
                POSIT_LUTYPE bfrac1 = POSIT_FRAC_TYPE_MSB | (b.fraction >> 1);
                auto exp = a.exponent - b.exponent + (afrac < bfrac1 ? -1 : 0);
                POSIT_LUTYPE bfrac = afrac < bfrac1 ? (bfrac >> 1) : bfrac1;
                /*
                if (afrac < bfrac) {
                    exp--;
                    bfrac >>= 1;
                }
                */

                return Unpacked(exp,  (((typename nextinttype<FT>::type)afrac) << POSIT_FRAC_TYPE_SIZE_BITS) / bfrac,a.negativeSign ^ b.negativeSign);
            }
            case UnpackedDualSel(Zero,Zero):
            case UnpackedDualSel(Infinity,Infinity):
                return nan();
            case UnpackedDualSel(Zero,Infinity):
                return zero();
            case UnpackedDualSel(Zero,Regular):
            case UnpackedDualSel(Infinity,Zero):
                return a;
            case UnpackedDualSel(Regular,Zero):
                return Unpacked(Unpacked::Infinity,a.negativeSign);
            default: // case UnpackedDualSel(Infinity,Regular):
                return (a.negativeSign^b.negativeSign)? ninfinity() : pinfinity();
        }
	}
    
    
	friend std::ostream & operator << (std::ostream & ons, Unpacked const & o)
    {
        switch(o.type)
        {
            case Unpacked::Regular:
                ons << "up(" << (o.negativeSign? "-":"+") << " exp (dec) = " << std::dec << typename printableinttype<const ET>::type(o.exponent) << " fraction (hex) = " << std::hex << typename printableinttype<const FT>::type(o.fraction) << " (bin) = " << std::dec << (std::bitset<sizeof(o.fraction)*8>(o.fraction)) << ")";
                break;
            case Unpacked::Infinity:
                ons << (o.negativeSign ? "up(-infinity)" : "up(+infinity)");
                break;
            case Unpacked::NaN:
                ons << "up(nan)";
                break;
            case Unpacked::Zero:
                ons << "up(0)";
                break;
        }
        return ons;
    }


};



// https://www.h-schmidt.net/FloatConverter/IEEE754.html
template <class FT, class ET>
template <class Trait>
CONSTEXPR14 void Unpacked<FT,ET>::unpack_xfloati(typename Trait::holder_t value)
{
    ET rawexp = bitset_getT(value,Trait::fraction_bits,Trait::exponent_bits) ;
	type = Regular;
    negativeSign = value & (((typename Trait::holder_t)1) << (Trait::data_bits-1));
	exponent = rawexp - Trait::exponent_bias; // ((un.u >> Trait::fraction_bits) & Trait::exponent_mask)

	//std::cout  << "un.u is " << std::hex <<un.u << " for " << value <<  std::endl;
	//std::cout << std::dec << "float trait: fraction bits " << Trait::fraction_bits << " exponent bits " << Trait::exponent_bits << " bias " << Trait::exponent_bias << " mask " << std::hex << Trait::exponent_mask<< std::endl;
	//std::cout << std::hex << "exponent output " << std::hex << exponent  << " " << std::dec << exponent << " fraction " << std::hex << fraction << std::endl;

	// fractional part is LSB of the holder_t and of length 
    fraction = cast_right_to_left<typename Trait::holder_t,Trait::fraction_bits,FT,POSIT_FRAC_TYPE_SIZE_BITS>()(value);

	//if(POSIT_FRAC_TYPE_SIZE_BITS < Trait::fraction_bits)
	//	fraction = bitset_getT(value,0,Trait::fraction_bits) >> (Trait::fraction_bits-POSIT_FRAC_TYPE_SIZE_BITS);
	//else
	//	fraction = ((POSIT_LUTYPE)bitset_getT(value,0,Trait::fraction_bits)) << (POSIT_FRAC_TYPE_SIZE_BITS-Trait::fraction_bits);

	// stored exponent: 0, x, exponent_mask === 0, any, infinity
	// biased: -max, -max+1, ..., max, max+1 === 0, min, ..., max, infinity
	if(rawexp == ((1 << Trait::exponent_bits)-1)) // AKA 128 for single
	{
		if(fraction == 0)
		{
			type = Infinity;
			return;
		}
		else
		{
			type = NaN;
			return;
		}
	}
	else if (rawexp == 0)
    {
        normalize();
	}
}

template <int abits, class AT, int bbits, class BT, bool abits_gt_bbits, AT msb>
struct fraction_bit_extract
{
};

template <int abits, class AT, int bbits, class BT,AT msb>
struct fraction_bit_extract<abits,AT,bbits,BT,true,msb>
{
    static constexpr BT packdenorm(AT fraction)
    {
        // expand the fractiona part
        return (msb | (fraction >> 1)) >> (abits-bbits);
    }

    static constexpr BT pack(AT fraction)
    {
        return bitset_getT(fraction,abits-bbits,bbits);
    }
};

template <int abits, class AT, int bbits, class BT, AT msb>
struct fraction_bit_extract<abits,AT,bbits,BT,false,msb>
{
    static constexpr BT packdenorm(AT fraction)
    {
        return ((BT) (msb | (fraction >> 1)) << (bbits-abits));
    }

    static constexpr BT pack(AT fraction)
    {
        return ((BT)fraction) << (bbits-abits);
    }
};

template <class FT,class ET>
template <class Trait>
CONSTEXPR14 typename Trait::holder_t Unpacked<FT,ET>::pack_xfloati() const
{
	switch(type)
	{
		case Infinity:
			return negativeSign ? Trait::ninfinity_h : Trait::pinfinity_h;
		case Zero:
			return 0;
		case NaN:
			return Trait::nan_h;; // it will cast to double TODO: it will cast to value_t
		default:
			break;
	}

	largest_type<ET,typename int_least_bits<Trait::exponent_bits>::type > fexp = exponent;
    fexp += Trait::exponent_bias; 

	// left aligned
	typename Trait::holder_t fexpbits = 0;
	typename Trait::holder_t ffracbits = 0;

	if (fexp > Trait::exponent_max) // AKA 254 for single
	{
		// overflow, set maximum value
		fexpbits = ((typename Trait::holder_t)Trait::exponent_max) << (Trait::fraction_bits); // AKA 254 and 23
		ffracbits = -1;
	}
	else if (fexp < 1) {
		// fraction is stored POSIT_FRAC_TYPE_SIZE_BITS and we need to
		// underflow, pack as denormal
		if (fraction != 0) {
			// shrink expand fractional part with 1. as denormal
            ffracbits = fraction_bit_extract<POSIT_FRAC_TYPE_SIZE_BITS,FT,Trait::fraction_bits,typename Trait::holder_t,(POSIT_FRAC_TYPE_SIZE_BITS > Trait::fraction_bits),POSIT_FRAC_TYPE_MSB>::packdenorm(fraction);
			// use denormalization
			ffracbits >>= -fexp;
		}
	}
	else
	{
		fexpbits = ((typename Trait::holder_t)(fexp & Trait::exponent_mask)) << (Trait::fraction_bits); // AKA 0xFF and 23 for single
        ffracbits = fraction_bit_extract<POSIT_FRAC_TYPE_SIZE_BITS,FT,Trait::fraction_bits,typename Trait::holder_t,(POSIT_FRAC_TYPE_SIZE_BITS > Trait::fraction_bits),POSIT_FRAC_TYPE_MSB>::pack(fraction);
	}

	typename Trait::holder_t value = ffracbits|fexpbits;
	if(negativeSign) {
		value |=  Trait::signbit; // add sign
	}

	// don't underflow to zero?
	if (value != 0 && (value << 1) == 0) {
		value++;
	}
	return value;
}


