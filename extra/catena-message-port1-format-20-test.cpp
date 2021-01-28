/*

Module:	message-port1-format-20-test.cpp

Function:
	Test vector generator for port 1, format 0x20, 21

Copyright and License:
	This file copyright (C) 2019 by

		MCCI Corporation
		3520 Krums Corners Road
		Ithaca, NY  14850

	See accompanying LICENSE file for copyright and license information.

Author:
	Terry Moore, MCCI Corporation	July 2019

*/

// To build:
//  Open a Visual Studio 2019 C++ command line window. Then:
//
//  C> cl /EHsc catena-message-port1-format-20-test.cpp
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

enum class OutputFormat
    {
    Bytes, Yaml
    };

//--- globals
OutputFormat gOutputFormat = OutputFormat::Bytes;
std::string key;
std::string value;

template <typename T>
struct val
    {
    bool fValid;
    T v;
    };

struct env
    {
    float t;
    float p;
    float rh;
    };

struct pm
    {
    float m1_0;
    float m2_5;
    float m10;
    };

struct dust
    {
    float m0_3, m0_5, m1_0, m2_5, m5, m10;
    };

struct Measurements
    {
    val<float> Vbat;
    val<float> Vsys;
    val<float> Vbus;
    val<std::uint8_t> Boot;
    val<env> Env;
    val<pm> Pm;
    val<dust> Dust;
    val<std::uint16_t> TVOC;
    };

uint16_t
LMIC_f2uflt16(
        float f
        )
        {
        if (f < 0.0)
                return 0;
        else if (f >= 1.0)
                return 0xFFFF;
        else
                {
                int iExp;
                float normalValue;

                normalValue = std::frexp(f, &iExp);

                // f is supposed to be in [0..1), so useful exp
                // is [0..-15]
                iExp += 15;
                if (iExp < 0)
                        // underflow.
                        iExp = 0;

                // bits 15..12 are the exponent
                // bits 11..0 are the fraction
                // we conmpute the fraction and then decide if we need to round.
                uint16_t outputFraction = std::ldexp(normalValue, 12) + 0.5;
                if (outputFraction >= (1 << 12u))
                        {
                        // reduce output fraction
                        outputFraction = 1 << 11;
                        // increase exponent
                        ++iExp;
                        }

                // check for overflow and return max instead.
                if (iExp > 15)
                        return 0xFFFF;

                return (uint16_t)((iExp << 12u) | outputFraction);
                }
        }

std::uint16_t encode16s(float v)
    {
    float nv = std::floor(v + 0.5f);

    if (nv > 32767.0f)
        return 0x7FFFu;
    else if (nv < -32768.0f)
        return 0x8000u;
    else
        {
        return (std::uint16_t) std::int16_t(nv);
        }
    }

std::uint16_t encode16u(float v)
    {
    float nv = std::floor(v + 0.5f);
    if (nv > 65535.0f)
        return 0xFFFFu;
    else if (nv < 0.0f)
        return 0;
    else
        {
        return std::uint16_t(nv);
        }
    }

std::uint16_t encodeV(float v)
    {
    return encode16s(v * 4096.0f);
    }

std::uint16_t encodeT(float v)
    {
    return encode16s(v * 256.0f);
    }

std::uint16_t encodeP(float v)
    {
    return encode16u(v * 25.0f);
    }

std::uint16_t encodeRH(float v)
    {
    return encode16u(v * 65535.0f / 100.0f);
    }

std::uint16_t encodePM(float v)
    {
    return encode16u(LMIC_f2uflt16(v / 65536.0f));
    }

std::uint16_t encodeDust(float v)
    {
    return encode16u(LMIC_f2uflt16(v / 65536.0f));
    }

std::uint16_t encodeTVOC(std::uint16_t v)
    {
    return v;
    }

class Buffer : public std::vector<std::uint8_t>
    {
public:
    Buffer() : std::vector<std::uint8_t>() {};

    void push_back_be(std::uint16_t v)
        {
        this->push_back(std::uint8_t(v >> 8));
        this->push_back(std::uint8_t(v & 0xFF));
        }
    };

void encodeMeasurement(Buffer &buf, Measurements &m)
    {
    std::uint8_t flags = 0;

    // sent the type byte
    buf.clear();
    buf.push_back(0x20);
    buf.push_back(0u); // flag byte.

    // put the fields
    if (m.Vbat.fValid)
        {
        flags |= 1 << 0;
        buf.push_back_be(encodeV(m.Vbat.v));
        }

    if (m.Vsys.fValid)
        {
        flags |= 1 << 1;
        buf.push_back_be(encodeV(m.Vsys.v));
        }

    if (m.Vbus.fValid)
        {
        flags |= 1 << 2;
        buf.push_back_be(encodeV(m.Vbus.v));
        }

    if (m.Boot.fValid)
        {
        flags |= 1 << 3;
        buf.push_back(m.Boot.v);
        }

    if (m.Env.fValid)
        {
        flags |= 1 << 4;

        buf.push_back_be(encodeT(m.Env.v.t));
        buf.push_back_be(encodeP(m.Env.v.p));
        buf.push_back_be(encodeRH(m.Env.v.rh));
        }

    if (m.Pm.fValid)
        {
        flags |= 1 << 5;

        buf.push_back_be(encodePM(m.Pm.v.m1_0));
        buf.push_back_be(encodePM(m.Pm.v.m2_5));
        buf.push_back_be(encodePM(m.Pm.v.m10));
        }

    if (m.Dust.fValid)
        {
        flags |= 1 << 6;

        buf.push_back_be(encodeDust(m.Dust.v.m0_3));
        buf.push_back_be(encodeDust(m.Dust.v.m0_5));
        buf.push_back_be(encodeDust(m.Dust.v.m1_0));
        buf.push_back_be(encodeDust(m.Dust.v.m2_5));
        buf.push_back_be(encodeDust(m.Dust.v.m5));
        buf.push_back_be(encodeDust(m.Dust.v.m10));
        }

    if (m.TVOC.fValid)
        {
        flags |= 1 << 7;
        buf.push_back_be(encodeTVOC(m.TVOC.v));
        }

    // update the flags
    buf.data()[1] = flags;
    }

void logMeasurement(Measurements &m)
    {
    class Padder {
    public:
        Padder() : m_first(true) {}
        const char *get() {
            if (this->m_first)
                {
                this->m_first = false;
                return "";
                }
            else
                return " ";
            }
        const char *nl() {
            return this->m_first ? "" : "\n";
            }
    private:
        bool m_first;
    } pad;

    std::cout << std::dec;

    // put the fields
    if (m.Vbat.fValid)
        {
        std::cout << pad.get() << "Vbat " << m.Vbat.v;
        }

    if (m.Vsys.fValid)
        {
        std::cout << pad.get() << "Vsys " << m.Vsys.v;
        }

    if (m.Vbus.fValid)
        {
        std::cout << pad.get() << "Vbus " << m.Vbus.v;
        }

    if (m.Boot.fValid)
        {
        std::cout << pad.get() << "Boot " << unsigned(m.Boot.v);
        }

    if (m.Env.fValid)
        {
        std::cout << pad.get() << "Env " << m.Env.v.t << " "
                                         << m.Env.v.p << " "
                                         << m.Env.v.rh;
        }

    if (m.Pm.fValid)
        {
        std::cout << pad.get() << "Pm " << m.Pm.v.m1_0 << " "
                                        << m.Pm.v.m2_5 << " "
                                        << m.Pm.v.m10;
        }

    if (m.Dust.fValid)
        {
        std::cout << pad.get() << "Dust " << m.Dust.v.m0_3 << " "
                                          << m.Dust.v.m0_5 << " "
                                          << m.Dust.v.m1_0 << " "
                                          << m.Dust.v.m2_5 << " "
                                          << m.Dust.v.m5   << " "
                                          << m.Dust.v.m10;
        }

    if (m.TVOC.fValid)
        {
        std::cout << pad.get() << "TVOC " << m.TVOC.v
                                           ;
        }

    // make the syntax cut/pastable.
    std::cout << pad.get() << ".\n";
    }

void putTestVector(Measurements &m)
    {
    Buffer buf {};
    logMeasurement(m);
    encodeMeasurement(buf, m);
    bool fFirst;

    fFirst = true;
    if (gOutputFormat == OutputFormat::Bytes)
        {
        for (auto v : buf)
            {
            if (! fFirst)
                std::cout << ' ';
            fFirst = false;
            std::cout.width(2);
            std::cout.fill('0');
            std::cout << std::hex << unsigned(v);
            }
        std::cout << '\n';
        std::cout << "length: " << std::dec << buf.end() - buf.begin() << '\n';
        }
    else if (gOutputFormat == OutputFormat::Yaml)
        {
        auto const sLeft = "  ";
        std::cout << "  examples:" << '\n'
                  << "    - description: XXX\n"
                  << "      input:\n"
                  << "        fPort: XXX\n"
                  << "        bytes: [";

        for (auto v : buf)
            {
            if (! fFirst)
                std::cout << ", ";
            fFirst = false;
            std::cout << std::dec << unsigned(v);
            }

        std::cout << "]\n"
                  << "      output:\n"
                  << "        data:\n"
                  << "          JSON-HERE\n"
                  ;
        }
    }

int main(int argc, char **argv)
    {
    Measurements m {0};
    Measurements m0 {0};
    bool fAny;

    if (argc > 1)
        {
        std::string opt;
        opt = argv[1];
        if (opt == "--yaml")
            {
            std::cout << "(output in yaml format)\n";
            gOutputFormat = OutputFormat::Yaml;
            }
        else
            {
            std::cout << "invalid option ignored: " << opt << '\n';
            }
        }

    std::cout << "Input a line with name/values pairs\n";

    fAny = false;
    while (std::cin.good())
        {
        bool fUpdate = true;
        key.clear();

        std::cin >> key;

        if (key == "Vbat")
            {
            std::cin >> m.Vbat.v;
            m.Vbat.fValid = true;
            }
        else if (key == "Vsys")
            {
            std::cin >> m.Vsys.v;
            m.Vsys.fValid = true;
            }
        else if (key == "Vbus")
            {
            std::cin >> m.Vbus.v;
            m.Vbus.fValid = true;
            }
        else if (key == "Boot")
            {
            std::uint32_t nonce;
            std::cin >> nonce;
            m.Boot.v = (std::uint8_t) nonce;
            m.Boot.fValid = true;
            }
        else if (key == "Env")
            {
            std::cin >> m.Env.v.t >> m.Env.v.p >> m.Env.v.rh;
            m.Env.fValid = true;
            }
        else if (key == "Pm")
            {
            std::cin >> m.Pm.v.m1_0 >> m.Pm.v.m2_5 >> m.Pm.v.m10;
            m.Pm.fValid = true;
            }
        else if (key == "Dust")
            {
            std::cin >> m.Dust.v.m0_3
                    >> m.Dust.v.m0_5
                    >> m.Dust.v.m1_0
                    >> m.Dust.v.m2_5
                    >> m.Dust.v.m5
                    >> m.Dust.v.m10;
            m.Dust.fValid = true;
            }
        else if (key == "TVOC")
            {
            std::cin >> m.TVOC.v;
            m.TVOC.fValid = true;
            }
        else if (key == ".")
            {
            putTestVector(m);
            m = m0;
            fAny = false;
            fUpdate = false;
            }
        else if (key == "")
            /* ignore empty keys */
            fUpdate = false;
        else
            {
            std::cerr << "unknown key: " << key << "\n";
            fUpdate = false;
            }

        fAny |= fUpdate;
        }

    if (!std::cin.eof() && std::cin.fail())
        {
        std::string nextword;

        std::cin.clear(std::cin.goodbit);
        std::cin >> nextword;
        std::cerr << "parse error: " << nextword << "\n";
        return 1;
        }

    if (fAny)
        putTestVector(m);

    return 0;
    }
