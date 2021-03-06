//
// Copyright (C) 2012-2015  Aleksandar Zlateski <zlateski@mit.edu>
// ---------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "network/parallel/network.hpp"
#include "network/trivial/trivial_fft_network.hpp"
#include <zi/zargs/zargs.hpp>

using namespace znn::v4;

// g++ -std=c++11 src/cpp/ISBI2012.cpp -o isbi -DNDEBUG -O3 -DZNN_USE_FLOATS -DZNN_CUBE_POOL_LOCKFREE -I. -I./src/include -lpthread -lrt -ljemalloc -lfftw3f

namespace ISBI2012 {

template<typename F, typename T>
inline cube_p<T> load( std::string const & fname, vec3i const & sz )
{
    FILE* fvol = fopen(fname.c_str(), "r");

    STRONG_ASSERT(fvol);

    auto ret = get_cube<T>(sz);
    F v;

    for ( long_t x = 0; x < sz[0]; ++x )
        for ( long_t y = 0; y < sz[1]; ++y )
            for ( long_t z = 0; z < sz[2]; ++z )
            {
                static_cast<void>(fread(&v, sizeof(F), 1, fvol));
                (*ret)[x][y][z] = static_cast<T>(v) / 255;
            }

    fclose(fvol);

    return ret;
}

class ISBI2012
{
private:
    cube_p<real> image;
    cube_p<real> label;

    vec3i       size_       ;
    vec3i       in_sz_      ;
    vec3i       out_sz_     ;
    vec3i       half_in_sz_ ;
    vec3i       half_out_sz_;
    vec3i       margin_sz_  ;
    vec3i       set_sz_     ;


public:
    ISBI2012( std::string const & fname,
              vec3i const & sz,
              vec3i const & in_sz,
              vec3i const & out_sz)
        : size_(sz)
        , in_sz_(in_sz)
        , out_sz_(out_sz)
    {
        image = load<double, real>(fname + ".image", sz);
        label = load<double, real>(fname + ".label", sz);

        half_in_sz_  = in_sz_/vec3i(2,2,2);
        half_out_sz_ = out_sz_/vec3i(2,2,2);

        // margin consideration for even-sized input
        margin_sz_ = half_in_sz_;
        if ( in_sz_[0] % 2 == 0 ) --(margin_sz_[0]);
        if ( in_sz_[1] % 2 == 0 ) --(margin_sz_[1]);
        if ( in_sz_[2] % 2 == 0 ) --(margin_sz_[2]);

        set_sz_ = size_ - margin_sz_ - half_in_sz_;
    }

    std::pair<cube_p<real>, cube_p<real>> get_sample()
    {
        vec3i loc = vec3i( half_in_sz_[0] + (rand() % set_sz_[0]),
                           half_in_sz_[1] + (rand() % set_sz_[1]),
                           half_in_sz_[2] + (rand() % set_sz_[2]));

        std::pair<cube_p<real>,cube_p<real>> ret;

        ret.first  = crop(*image, loc - half_in_sz_, in_sz_);
        ret.second = crop(*label, loc - half_out_sz_, out_sz_);

        return ret;
    }

  std::tuple<real,real,cube_p<real>> square_loss( cube<real> const & cprop,
                                                  cube<real> const & clab )
    {
        std::tuple<real,real,cube_p<real>> ret;
        std::get<0>(ret) = 0;
        std::get<1>(ret) = 0;
        std::get<2>(ret) = get_copy(cprop);

        real* grad = std::get<2>(ret)->data();
        const real* lab  = clab.data();

        long_t n = clab.num_elements();

        for ( long_t i = 0; i < n; ++i )
        {
            std::get<1>(ret) += ( grad[i] > 0.5 ? ( lab[i] > 0.5 ? 0 : 1 ) : ( lab[i] > 0.5 ? 1 : 0 ) );
            grad[i] -= lab[i];
            std::get<0>(ret) += grad[i]*grad[i];
            grad[i] *= 2;
        }

        return ret;
    }

}; // class ISBI2012


} // namespace ISBI2012

int main(int argc, char** argv)
{
  srand(1234);

    std::vector<options> nodes;
    std::vector<options> edges;

    parse_net_file(nodes, edges, argv[1]);

    int64_t x = 9;
    int64_t y = 9;
    int64_t z = 9;

    if ( argc >= 5 )
    {
        x = atoi(argv[2]);
        y = atoi(argv[3]);
        z = atoi(argv[4]);
    }

    size_t tc = std::thread::hardware_concurrency();

    if ( argc == 6 )
    {
        tc = atoi(argv[5]);
    }

    size_t freq = 100;

    if ( argc == 7 )
    {
        freq = atoi(argv[6]);
    }

    vec3i out_sz(x,y,z);

    if ( argc == 8 )
    {
        size_t n_opt = 0;
        if ( n_opt = atoi(argv[7]) )
            parallel_network::network::optimize(nodes,edges,out_sz,tc,n_opt);
    }

    parallel_network::network net(nodes,edges,out_sz,tc);
    net.set_eta(0.01);

    vec3i in_sz = out_sz + net.fov() - vec3i::one;


    ISBI2012::ISBI2012 isbi( "./dataset/ISBI2012/data/batch1",
                             vec3i(30,256,256), in_sz, out_sz );

    zi::wall_timer wt;
    wt.reset();

    real err = 0;
    real cls = 0;
    for ( long_t i = 0; i < 1000000; )
    {
        std::map<std::string, std::vector<cube_p<real>>> insample;
        std::map<std::string, std::vector<cube_p<real>>> outsample;

        insample["input"].resize(1);
        outsample["output"].resize(1);

        auto sample = isbi.get_sample();
        insample["input"][0] = sample.first;

        auto prop = net.forward(std::move(insample));

        //std::cout << *sample.second << *prop["nl6"][0] << std::endl;

        auto grad = isbi.square_loss(*(prop["output"][0]), *sample.second);
        err += std::get<0>(grad);
        cls += std::get<1>(grad);

        outsample["output"][0] = std::get<2>(grad);

        net.backward(std::move(outsample));

        ++i;
        if ( i % freq == 0 )
        {
            err /= freq;
            err /= x;
            err /= y;
            err /= z;
            cls /= freq; cls /= ( x * y * z );
            std::cout << "Iteration: " << i << " done, elapsed: "
                      << wt.elapsed<double>() << ", sqerr: " << err
                      << " clserr: " << cls << std::endl;
            err = 0;
            cls = 0;
            wt.reset();

            // // show prop
            // cube_p<real> outcube_p = prop["output"][0];
            // std::cout<<"prop output cube:"<<std::endl;
            // for(std::size_t i=0; i<outcube_p->num_elements(); i++)
            // 	std::cout<< outcube_p->data()[i]<< ",";
            // std::cout<<std::endl;

            // // show gradient
            // cube_p<real> grad_p = std::get<2>(grad);
            // std::cout<<"gradient:"<<std::endl;
            // for(std::size_t i=0; i<grad_p->num_elements(); i++)
            //     std::cout<< grad_p->data()[i]<< ",";
            // std::cout<<std::endl;
        }
    }

}
