#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

#include <components/esm/esmreader.hpp>
#include <components/esm/esmwriter.hpp>

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;


struct File
{
    ESM::Header mHeader;

    struct Subrecord
    {
        std::string mName;
        std::vector<unsigned char> mData;
    };

    struct Record
    {
        std::string mName;
        std::vector<Subrecord> mSubrecords;
    };

    std::vector<Record> mRecords;
};

void read(const std::string& filename, File& file)
{
    ESM::ESMReader esm;
    esm.open(filename);

    file.mHeader = esm.getHeader();

    while (esm.hasMoreRecs())
    {
        ESM::NAME n = esm.getRecName();
        esm.getRecHeader();

        File::Record rec;
        rec.mName = n.toString();
        while (esm.hasMoreSubs())
        {
            File::Subrecord sub;
            esm.getSubName();
            esm.getSubHeader();
            sub.mName = esm.retSubName().toString();
            sub.mData.resize(esm.getSubSize());
            esm.getExact(&sub.mData[0], sub.mData.size());
            rec.mSubrecords.push_back(sub);
        }
        file.mRecords.push_back(rec);
    }
}

int main(int argc, char** argv)
{
    try
    {
        bpo::options_description desc("Syntax: openmw-essimporter <options> infile.ess outfile.omwsave\nAllowed options");
        bpo::positional_options_description p_desc;
        desc.add_options()
            ("esmfile", bpo::value<std::string>(), "Input ESM file")
            ("espfile", bpo::value<std::string>(), "Input ESP file")
            ("outputesm", bpo::value<std::string>(), "Output ESM file")
        ;
        p_desc.add("esmfile", 1).add("espfile", 1).add("outputesm", 1);

        bpo::variables_map variables;

        bpo::parsed_options parsed = bpo::command_line_parser(argc, argv)
            .options(desc)
            .positional(p_desc)
            .run();

        bpo::store(parsed, variables);

        bpo::notify(variables);

        std::string esmfile = variables["esmfile"].as<std::string>();
        std::string espfile = variables["espfile"].as<std::string>();
        std::string outputesm = variables["outputesm"].as<std::string>();

        File file1;
        read(esmfile, file1);

        File file2;
        read(espfile, file2);

        file1.mRecords.insert(file1.mRecords.end(), file2.mRecords.begin(), file2.mRecords.end());

        ESM::ESMWriter writer;
        writer.setFormat(file1.mHeader.mFormat);
        writer.setVersion(file1.mHeader.mData.version);
        writer.setType(file1.mHeader.mData.type);
        writer.setAuthor(file1.mHeader.mData.author.toString());
        writer.setDescription(file1.mHeader.mData.desc.toString());

        bfs::ofstream stream(outputesm, std::ios::binary);

        writer.save(stream);

        const std::vector<File::Record>& records = file1.mRecords;

        for (std::vector<File::Record>::const_iterator recIt = records.begin(); recIt != records.end(); ++recIt)
        {
            writer.startRecord(recIt->mName);

            for (std::vector<File::Subrecord>::const_iterator subrecIt = recIt->mSubrecords.begin(); subrecIt != recIt->mSubrecords.end(); ++subrecIt)
            {
                writer.startSubRecord(subrecIt->mName);
                writer.write((char*)&subrecIt->mData[0], subrecIt->mData.size());
                writer.endRecord(subrecIt->mName);
            }

            writer.endRecord(recIt->mName);
        }

        writer.close();
    }
    catch (std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
