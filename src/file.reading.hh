#include <vector>
#include <string>
#include <memory>
#include <ostream>
#include <unordered_map>

namespace file_reading {

struct chrpos {
    int chr;
    int pos;
    bool operator< (chrpos const & other) const {
        if(chr < other.chr) return true;
        if(chr > other.chr) return false;

        return pos < other.pos;
    }
    bool operator> (chrpos const & other) const {
        if(chr > other.chr) return true;
        if(chr < other.chr) return false;

        return pos > other.pos;
    }
    bool operator==(chrpos const & other) const {
        return(chr == other.chr
            && pos == other.pos);
    }
    bool operator!=(chrpos const & other) const {
        return !(*this == other);
    }
};
} // end that namespace, in order to define std:: hash

template<>
struct :: std:: hash < file_reading:: chrpos >
{
    auto   operator() (file_reading:: chrpos const &crps) const
        noexcept(noexcept(
               hash< decltype(crps.chr) >{}(crps.chr)
             + hash< decltype(crps.pos) >{}(crps.pos)
                ))
        -> decltype(
               hash< decltype(crps.chr) >{}(crps.chr)
             + hash< decltype(crps.pos) >{}(crps.pos)
                )
    {
        return hash< decltype(crps.chr) >{}(crps.chr)
             + hash< decltype(crps.pos) >{}(crps.pos);
    }
};


namespace file_reading { // reopen this namespace

inline
std:: ostream& operator<<(std:: ostream &o, chrpos const &c) {
    o
        << "chr"
        << c.chr
        << ':'
        << c.pos;
    return o;
}

struct AnyFile_I {
    virtual int         number_of_snps     ()        const = 0;
    virtual std::string get_SNPname        (int)     const = 0;
    virtual chrpos      get_chrpos         (int)     const = 0;
    virtual std::string get_allele_ref     (int)     const = 0;
    virtual std::string get_allele_alt     (int)     const = 0;
    //virtual char        get_delimiter      ()       const = 0;
};

struct Genotypes_I : public AnyFile_I {
    virtual std::pair<
             std::vector<uint8_t>
            ,std::vector<uint8_t>
        > get_calls          (int)     const = 0;
};
struct Effects_I : public AnyFile_I {
    virtual void        set_chrpos         (int, chrpos)  = 0; // so that we can fill them in from the ref data
    virtual void        sort_my_entries    ()             = 0;
    virtual double      get_z              (int) const    = 0;
};

using GenotypeFileHandle = std:: shared_ptr<Genotypes_I const>;
using GwasFileHandle     = std:: shared_ptr<Effects_I   const>;
using GwasFileHandle_NONCONST     = std:: shared_ptr<Effects_I>;

template<typename GWASorREF> // GwasFileHandle *or* GenotypeFileHandle
struct SNPiterator
: public std::iterator<std:: random_access_iterator_tag, chrpos>
{
    GWASorREF m_gfh;
    int                m_line_number; // 0 means the first SNP that was read, 1 the second, and so on

    static_assert(std::is_same< GWASorREF , GenotypeFileHandle >{}
              ||  std::is_same< GWASorREF , GwasFileHandle_NONCONST     >{}
              ||  std::is_same< GWASorREF , GwasFileHandle     >{}
              , "");

    // Constructor
                        SNPiterator(GWASorREF gfh, int line_number) : m_gfh(gfh), m_line_number(line_number) {}


    // operators
    SNPiterator &       operator++();
    bool                operator==(SNPiterator const & other) const;
    bool                operator!=(SNPiterator const & other) const;
    bool                operator< (SNPiterator const & other) const;
    bool                operator>=(SNPiterator const & other) const;
    int                 operator- (SNPiterator const & other) const;
    chrpos              operator* ()                          const;
    SNPiterator &       operator+=(long int            ran);


    chrpos get_chrpos() const {
        return m_gfh->get_chrpos(m_line_number);
    }
    std:: string      get_SNPname() const {
        return m_gfh->get_SNPname(m_line_number);
    }
    std:: string      get_allele_ref() const {
        return m_gfh->get_allele_ref(m_line_number);
    }
    std:: string      get_allele_alt() const {
        return m_gfh->get_allele_alt(m_line_number);
    }

    template<typename T = void>
    auto   set_chrpos(chrpos crps)
        -> decltype ( (std::declval<T>(),m_gfh) -> set_chrpos(m_line_number, crps) )
    {
        return                           m_gfh  -> set_chrpos(m_line_number, crps);
    }
    template<typename T = void>
    auto   get_calls()  const
        -> decltype ( (std::declval<T>(),m_gfh) -> get_calls(m_line_number) )
    {
        return                           m_gfh  -> get_calls(m_line_number);
    }
    template<typename T = void>
    auto   get_z()  const
        -> decltype ( (std::declval<T>(),m_gfh) -> get_z(m_line_number) )
    {
        return                           m_gfh  -> get_z(m_line_number);
    }
};

template<typename GWASorREF>
SNPiterator<GWASorREF> begin_from_file(GWASorREF fh) {
    return {fh, 0};
}
template<typename GWASorREF>
SNPiterator<GWASorREF>   end_from_file(GWASorREF fh) {
    return {fh, fh->number_of_snps()};
}

GenotypeFileHandle      read_in_a_raw_ref_file(std:: string file_name);
GwasFileHandle_NONCONST read_in_a_gwas_file(std:: string file_name);
void update_positions_by_comparing_to_another_set( GwasFileHandle_NONCONST gwas, std:: unordered_map<std:: string, file_reading:: chrpos> const & m );

struct CacheOfRefPanelData {
private:
    GenotypeFileHandle m_rfh;
    std:: unordered_map< chrpos , std::vector<int> > m_cache_of_z12;
    std:: unordered_map< int    , std::vector<int> > m_cache_of_z12_line_number;

public:
                                    CacheOfRefPanelData     (GenotypeFileHandle rfh) : m_rfh(rfh) {}

    std::vector<int>                lookup_one_chr_pos      (chrpos crps);
    std:: vector<int>               lookup_one_ref_get_calls(SNPiterator<GenotypeFileHandle> it);
};

} // namespace file_reading