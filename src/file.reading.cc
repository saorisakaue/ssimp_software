#include "file.reading.hh"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <map>
#include <cstring>

#include <gzstream.h>

#include <gsl/gsl_cdf.h>

#include "bits.and.pieces/DIE.hh"
#include "bits.and.pieces/PP.hh"
#include "bits.and.pieces/utils.hh"
#include "range/range_view.hh"
#include "range/range_action.hh"
#include "range/range_from.hh"

#include "vcfGTz_reader.hh"

namespace action = range:: action;
namespace view   = range:: view  ;
namespace from   = range:: from  ;

using std:: ifstream;
using std:: string;
using std:: vector;
using std:: map;
using std:: cout;
using utils:: ssize;
using utils:: tokenize;
using utils:: nice_operator_shift_left;
using utils:: save_ostream_briefly;
using utils:: print_type;
using vcfGTz:: special_encoder_for_list_of_GT_fields;

#define LOOKUP(hd, fieldname, vec) lookup(hd . fieldname, vec, #fieldname)

namespace file_reading {
    SNPiterator    &       SNPiterator   :: operator++()        {
        assert(m_line_number < m_gfh->number_of_snps());
        ++m_line_number;
        return *this;
    }
    SNPiterator    &       SNPiterator   :: operator--()        {
        assert(m_line_number > 0);
        --m_line_number;
        return *this;
    }
    bool                SNPiterator   :: operator==(SNPiterator    const & other) const {
        assert(m_gfh == other.m_gfh);
        return m_line_number == other.m_line_number;
    }
    bool                SNPiterator   :: operator!=(SNPiterator    const & other) const {
        return !(*this == other);
    }
    bool                SNPiterator   :: operator< (SNPiterator    const & other) const {
        assert(m_gfh == other.m_gfh);
        return m_line_number < other.m_line_number;
    }
    bool                SNPiterator   :: operator>=(SNPiterator    const & other) const { return !(*this < other); }
    int                 SNPiterator   :: operator- (SNPiterator    const & other) const {
        assert(m_gfh == other.m_gfh);
        return m_line_number - other.m_line_number;
    }
    chrpos              SNPiterator   :: operator* () const {
        return get_chrpos();
    }
    SNPiterator    &       SNPiterator   :: operator+=(long int            ran)   {
        m_line_number += ran;
        return *this;
    }

struct header_details {
    struct offset_and_name {
        int                  m_offset;
        string               m_name;

        offset_and_name() : m_offset(-1)
        {}
        offset_and_name(int offset, string name) : m_offset(offset), m_name(name)
        {}
        offset_and_name(offset_and_name const &) = default;
        offset_and_name(offset_and_name      &&) = default;

        void operator=(offset_and_name const & from) {
            if(m_offset != -1) {
                WARNING("Two fields with a similar name, using [" << m_name << "] and ignoring [" << from.m_name << "].");
                return;
            }
            assert(m_offset == -1);
            m_offset = from.m_offset;
            m_name   = from.m_name;
        }
        void operator=(offset_and_name && from) {
            return *this = from;
        }
    };

    char                     m_delimiter;
    offset_and_name    SNPname;
    offset_and_name    chromosome;
    offset_and_name    position;
    offset_and_name    allele_ref;
    offset_and_name    allele_alt;
    offset_and_name    effect_z;
    offset_and_name    effect_beta; // just for direction - we'll use this with 'p', if 'z' isn't present
    offset_and_name    effect_se;
    offset_and_name    effect_p;
    offset_and_name    effect_N;
};


// Some forward declarations

static
string          lookup( header_details:: offset_and_name const &on
                      , vector<string> const & all_split_up
                      , const char *fieldname
                      );
static
file_reading::
header_details   parse_header( string      const & header_line );
static bool is_in_this_list_CASEINSENSITIVE(string const & s, std:: initializer_list<char const *> candidates) ;
static
GwasFileHandle_NONCONST      read_in_a_gwas_file_simple(std:: string file_name);
char   decide_delimiter( string      const & header_line );

struct GwasLineSummary {
    int                      m_gwas_line_number;
    string                   m_SNPname;
    chrpos                   m_chrpos;
    string                   m_allele_alt;
    string                   m_allele_ref;
    double                   m_z;
    double                   m_N;
};

GwasFileHandle_NONCONST read_in_a_gwas_file(std:: string file_name) {
    // I really should detect the file-type.
    // But for now, we'll just assume a plain (non-gzipped) vcf file.
    return read_in_a_gwas_file_simple(file_name);
}

static
string          lookup( header_details:: offset_and_name const &on
                      , vector<string> const & all_split_up
                      , const char *fieldname
                      ) {
    int offset = on.m_offset;
    (offset >= 0 && offset < ssize(all_split_up)) || DIE("Cannot find ["<<fieldname<<"] field");

    return all_split_up.at(offset);
}

static
file_reading::
header_details   parse_header( string      const & header_line ) {
    header_details hd;

    hd.m_delimiter = decide_delimiter(header_line);
    auto header_names = tokenize(header_line, hd.m_delimiter);

    int field_counter = -1;
    for(auto & one_field_name : header_names) {
        ++field_counter;
        // go through each field name in turn and try to account for it
        if(false) {}
        else if(is_in_this_list_CASEINSENSITIVE(one_field_name, {
                         "ID"
                        ,"rnpid"
                        ,"snpid"
                        ,"rsid"
                        ,"MarkerName"
                        ,"snp"
                        ,"id1"
                        ,"marker"
                    })) {
            hd.SNPname = header_details:: offset_and_name(field_counter, one_field_name);
        }
        else if(is_in_this_list_CASEINSENSITIVE(one_field_name, {"#CHROM","chr","chromosome","chrm"})) {
            hd.chromosome = header_details:: offset_and_name(field_counter, one_field_name);
        }
        else if(is_in_this_list_CASEINSENSITIVE(one_field_name, {"POS","position","BP"})) {
            hd.position = header_details:: offset_and_name(field_counter, one_field_name);
        }
        else if(is_in_this_list_CASEINSENSITIVE(one_field_name, {
                         "REF"
                        ,"a1"
                        ,"Allele1"
                        ,"AlleleA"
                        ,"other_allele"
                    })) {
            hd.allele_ref = header_details:: offset_and_name(field_counter, one_field_name);
        }
        else if(is_in_this_list_CASEINSENSITIVE(one_field_name, {
                         "ALT"
                        ,"a2"
                        ,"Allele2"
                        ,"AlleleB"
                        ,"effect_allele"
                        })) {
            hd.allele_alt = header_details:: offset_and_name(field_counter, one_field_name);
        }
        else if(is_in_this_list_CASEINSENSITIVE(one_field_name, {"z.from.peff"
                                                ,"z"
                                                ,"stat"
                                                ,"zscore"
                                                ,"z.score"
                                                })) {
            hd.effect_z = header_details:: offset_and_name(field_counter, one_field_name);
        }
        else if(is_in_this_list_CASEINSENSITIVE(one_field_name, {"p"
                            ,"P-value"
                            ,"P.value"
                            ,"PVALUE"
                            ,"frequentist_add_pvalue"
                            ,"normal.score.p"
                            })) {
            hd.effect_p = header_details:: offset_and_name(field_counter, one_field_name);
        }
        else if(is_in_this_list_CASEINSENSITIVE(one_field_name, {"b","beta","ALT_EFFSIZE"
                    ,"frequentist_add_beta_1"
                    ,"normal.score.beta"
                    })) {
            // Note: this is only for the direction. We use this with effect_p, if effect_z isn't known
            hd.effect_beta = header_details:: offset_and_name(field_counter, one_field_name);
        }
        else if(is_in_this_list_CASEINSENSITIVE(one_field_name, {
                                                 "se"
                                                ,"frequentist_add_se_1"
                                                ,"normal.score.se"
                                                })) {
            hd.effect_se = header_details:: offset_and_name(field_counter, one_field_name);
        }
        else if(is_in_this_list_CASEINSENSITIVE(one_field_name, {
                             "n"
                            ,"NMISS"
                            ,"all_total"
                            })) {
            hd.effect_N = header_details:: offset_and_name(field_counter, one_field_name);
        }
        else {
        }
    }
    return hd;
}

/* The old case-sensitive version
static bool is_in_this_list(string const & s, std:: initializer_list<char const *> candidates) {
    for(auto cand : candidates) {
        if(s==cand)
            return true;
    }
    return false;
}
*/
static bool is_in_this_list_CASEINSENSITIVE(string const & s, std:: initializer_list<char const *> candidates) {
    for(auto cand : candidates) {
        if(std:: strlen(cand) != s.length())
            continue;
        bool same = true;
        for(size_t i = 0; i< s.length(); ++i) {
            if( std:: toupper(s.at(i))
             != std:: toupper(cand[i]) ){
                same=false;
                break;
            }
        }
        if(same)
            return true;
        //if(s==cand) return true;
    }
    return false;
}

char   decide_delimiter( string      const & header_line ) {
    // Which is comma, tab, or space, are most common here?

    size_t commas = std::count(header_line.begin(), header_line.end(), ',');
    size_t tabs   = std::count(header_line.begin(), header_line.end(), '\t');
    size_t spaces = std::count(header_line.begin(), header_line.end(), ' ');
    char delimiter = '\0';
    if(commas >  0 && tabs == 0 && spaces == 0) delimiter = ',';
    if(commas == 0 && tabs >  0 && spaces == 0) delimiter = '\t';
    if(commas == 0 && tabs == 0 && spaces >  0) delimiter = ' ';
    if(delimiter == '\0') {
        DIE("Couldn't decide what delimiter to use."
                <<  " #commas:" << commas
                << ", #tabs:"     << tabs
                << ", #spaces:"     << spaces
           );
    }
    return delimiter;
}

struct SimpleGwasFile : public file_reading:: Effects_I
{
    vector<GwasLineSummary> m_each_SNP_and_its_z;
    string                  m_underlying_file_name;
    char                    m_delimiter;
    header_details          m_header_details;

    virtual int         number_of_snps() const {
        return m_each_SNP_and_its_z.size();
    }
    virtual int         get_line_number    (int i)     const {
        return get_gls(i).m_gwas_line_number;
    }
    virtual std::string get_SNPname        (int i)     const {
        auto const & ols = get_gls(i);
        return ols.m_SNPname;
    }
    virtual chrpos      get_chrpos        (int i)     const {
        auto const & ols = get_gls(i);
        return ols.m_chrpos;
    }
    virtual std::string get_allele_ref        (int i)     const {
        auto const & ols = get_gls(i);
        return ols.m_allele_ref;
    }
    virtual std::string get_allele_alt        (int i)     const {
        auto const & ols = get_gls(i);
        return ols.m_allele_alt;
    }
    virtual double      get_z                 (int i)     const {
        auto const & ols = get_gls(i);
        return ols.m_z;
    }
    virtual double      get_N                 (int i)     const {
        auto const & ols = get_gls(i);
        return ols.m_N;
    }
    virtual void        set_chrpos        (int i, chrpos crps)  {
        assert(i>=0);
        assert(i<number_of_snps());
        auto & existing = m_each_SNP_and_its_z.at(i);
        //assert(existing.m_chrpos.chr == -1);
        //assert(existing.m_chrpos.pos == -1);
        existing.m_chrpos = crps;
    }
    virtual std::string get_column_name_allele_ref () const  { return m_header_details.allele_ref.m_name; }
    virtual std::string get_column_name_allele_alt () const  { return m_header_details.allele_alt.m_name; }
    virtual void        sort_my_entries   () {
        sort( m_each_SNP_and_its_z.begin()
            , m_each_SNP_and_its_z.end()
            , [](auto &l, auto &r) {
                return l.m_chrpos < r.m_chrpos;
            }
            );
    }
    virtual void        delete_snps_with_no_position() {
        m_each_SNP_and_its_z.erase(
            std::remove_if( m_each_SNP_and_its_z.begin(),
                            m_each_SNP_and_its_z.end(),
                            [](auto const & x) {
                                return x.m_chrpos == chrpos{-1,-1};
                            }),
            m_each_SNP_and_its_z.end()
        );
    }
    virtual int         delete_snps_with_identical_alleles() {
        auto delete_after_me = std::remove_if(
                            m_each_SNP_and_its_z.begin(),
                            m_each_SNP_and_its_z.end(),
                            [](auto const & x) {
                                return x.m_allele_alt == x.m_allele_ref;
                            });
        int count_those_removed = m_each_SNP_and_its_z.end() - delete_after_me;
        m_each_SNP_and_its_z.erase(delete_after_me, m_each_SNP_and_its_z.end());
        return count_those_removed;
    }

    GwasLineSummary  get_gls         (int i)     const {
        assert(i>=0);
        assert(i<number_of_snps());
        return m_each_SNP_and_its_z.at(i);
    }

};

static
GwasFileHandle_NONCONST      read_in_a_gwas_file_simple(std:: string file_name) {
    PP(file_name);

    gz:: igzstream f(file_name.c_str());
    (f.rdbuf() && f.rdbuf()->is_open()) || DIE("Can't find file [" << file_name << ']');

    string current_line;

    header_details hd;

    getline(f, current_line);
    assert(f);

    // current_line is now the first line, i.e. the header
    hd = parse_header(current_line);

    auto p = std:: make_shared<SimpleGwasFile>();
    p->m_underlying_file_name = file_name;
    p->m_delimiter            = hd.m_delimiter;
    p->m_header_details       = hd;

    int line_number = 1;

    while(1) {
        GwasLineSummary gls;
        getline(f, current_line);
	++line_number;
	gls.m_gwas_line_number = line_number;
        if(!f) {
            f.eof() || DIE("Error before reaching eof() in this file. Line number: " << line_number);
            break;
        }
        auto all_split_up = tokenize(current_line, hd.m_delimiter);
        try {
            gls.m_SNPname    =                           LOOKUP(hd, SNPname, all_split_up); // TODO: This should be optional
            gls.m_allele_alt =                           LOOKUP(hd, allele_alt, all_split_up);
            gls.m_allele_ref =                           LOOKUP(hd, allele_ref, all_split_up);
            gls.m_z          = [&](){
                if(hd.effect_z.m_offset != -1) {
                    return utils:: lexical_cast<double> (LOOKUP( hd, effect_z, all_split_up));
                }
                else { // no 'z' field. Must infer it from 'p' and 'beta', with 'beta' used only for 'direction'
                    auto beta   = utils:: lexical_cast<double> (LOOKUP( hd, effect_beta, all_split_up));
                    auto p      = utils:: lexical_cast<double> (LOOKUP( hd, effect_p   , all_split_up));
                    (p >= 0 && p <= 1)    || DIE("Your p should be between 0 and 1 [p=" << p << "]. Line number: " << line_number);
                    auto z_undir = gsl_cdf_gaussian_Pinv( p / 2.0 ,1);
                    assert(z_undir <= 0.0);
                    double z;
                    if(beta > 0)
                        z = -z_undir;
                    else
                        z =  z_undir;
                    //PP(p, z, beta);
                    return z;
                }
            }();
            gls.m_N          = [&](){
                if(hd.effect_N.m_offset != -1) {
                    return utils:: lexical_cast<double> (LOOKUP( hd, effect_N, all_split_up));
                }
                else {
                    return -1.0;
                }
            }();

            for(char & c : gls.m_allele_alt) { c = toupper(c); }
            for(char & c : gls.m_allele_ref) { c = toupper(c); }

            // Try to read in chromosome and position, but they may not be present
            // If missing, we'll fill them in much later from the reference panel
            gls.m_chrpos.chr = -1;
            gls.m_chrpos.pos = -1;
            if(hd.chromosome.m_offset != -1) {
                std::string chr = LOOKUP(hd, chromosome, all_split_up);
                if(chr == "X" || chr == "x")
                    chr = "23";
                gls.m_chrpos.chr = utils:: lexical_cast<int>( chr );
            }
            if(hd.position.m_offset != -1) {
                gls.m_chrpos.pos = utils:: lexical_cast<int>( LOOKUP(hd, position, all_split_up) );
            }

            p->m_each_SNP_and_its_z.push_back(gls);
        } catch (std:: invalid_argument &e) {
            WARNING( "Ignoring line [" << file_name << ":" << line_number << "], problem with the chromosome and/or position ["
                    << "SNPname:" << LOOKUP(hd, SNPname   , all_split_up)
                    //<< " chr:" << LOOKUP(hd, chromosome, all_split_up)
                    //<< " pos:" << LOOKUP(hd, position  , all_split_up)
                    << "], "
                    << e.what()
                    );
        }
    };

    /* Note: Sort them by position here */

    return p;
}

} // namespace file_reading
