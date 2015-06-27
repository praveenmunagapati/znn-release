#include "network/parallel/network.hpp"
#include "network/trivial/trivial_fft_network.hpp"
#include "network/trivial/trivial_network.hpp"
#include "network/helpers.hpp"
#include <zi/zargs/zargs.hpp>
#include <fstream>

using namespace znn::v4;

int main(int argc, char** argv)
{
    std::ofstream ofs;
    ofs.open (argv[1], std::ofstream::out | std::ofstream::app);

    std::vector<options> nodes, edges;

    int64_t W = atoi(argv[2]);
    int64_t D = atoi(argv[3]);

    nodes.resize(D+1);
    edges.resize(D);

    nodes[0].push("name", "input").push("type", "input").push("size", 1);

    for ( int64_t i = 0; i < D; ++i )
    {
        edges[i].push("name", i).push("type", "conv").push("init", "uniform")
            .push("size", "5,5,5").push("stride", "1,1,1")
            .push("input", i-1).push("output",i);
        nodes[i+1].push("name",i).push("type","transfer")
            .push("function","rectify_linear").push("size",W);
    }

    edges[0].push("input","input");
    edges[D-1].push("output","output");
    nodes[D].push("name","output");

    int64_t x = 9;
    int64_t y = 9;
    int64_t z = 9;

    if ( argc >= 7 )
    {
        x = atoi(argv[4]);
        y = atoi(argv[5]);
        z = atoi(argv[6]);
    }

    size_t tc = std::thread::hardware_concurrency();

    if ( argc >= 8 )
    {
        tc = atoi(argv[7]);
    }

    size_t nrnds = 31;
    if ( tc == 1 ) nrnds = 3;

    if ( argc >= 9 )
    {
        nrnds = atoi(argv[8]);
    }

    // for ( int i = 0; i < 10; ++i )
    // {
        auto res = parallel_network::network::speed_test
            (nodes, edges, {z,y,x}, tc, nrnds, 3);

        ofs << W << ", " << D << ", " << tc << ", " << res.first << ", " << res.second << ";" << std::endl;
        std::cout << W << ", " << D << ", " << tc << ", " << res.first << ", " << res.second
                  << " ( " << ( res.second * 100  / res.first ) << "% )"
                  << ";" << std::endl;
    // }
}