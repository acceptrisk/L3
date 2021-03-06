/*
The MIT License (MIT)

Copyright (c) 2015 Norman Wilson - Volcano Consultancy Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <L3/util/fifo.h>

#include <array>
#include <iostream>
#include <thread>

using namespace L3;

L3_CACHE_LINE size_t putSpinCount = 0;
L3_CACHE_LINE size_t getSpinCount = 0;
L3_CACHE_LINE size_t cursorSpinCount = 0;

template<size_t& counter>
struct Counter
{
    void operator()() { ++counter; }
};

using PutSpinCounter = Counter<putSpinCount>;
using GetSpinCounter = Counter<getSpinCount>;
using CursorSpinCounter = Counter<cursorSpinCount>;

typedef size_t Msg;

using DR = Fifo<Msg, 19>;
using PUT = DR::Put<PutSpinCounter, CursorSpinCounter>;
using GET = DR::Get<GetSpinCounter>;

DR buf;

constexpr size_t iterations = 100000000; // 100 Million.

std::array<Msg, iterations> msgs;

bool testSingleProducerSingleConsumer()
{
    int result = 0;

    std::thread producer(
        [&](){
            for(Msg i = 1; i < iterations; i++)
            {
                PUT p(buf);
                p = i;
            }
            std::cerr << "Prod done" << std::endl;
        });

    std::thread consumer(
        [&](){
            Msg previous = buf.get();
            for(size_t i = 1; i < iterations - 1; ++i)
            {
                Msg msg;
                {
                    msg = GET(buf);
                }
                msgs[i] = msg;
                long diff = (msg - previous) - 1;
                result += (diff * diff);
                previous = msg;
            }
            std::cerr << "Cons done" << std::endl;
        });


    producer.join();
    consumer.join();

    std::cout << "result: " << result << std::endl;

    std::cout << "putSpinCount    = " << putSpinCount << std::endl;
    std::cout << "getSpinCount    = " << getSpinCount << std::endl; 
    std::cout << "cursorSpinCount = " << cursorSpinCount << std::endl;

    Msg previous = 0;
    bool status = true;
    for(auto i: msgs)
    {
        if(i == 0)
        {
            continue;
        }
        if(previous >= i)
        {
            std::cout << "Wrong at: " << i << std::endl;
            status = false;
        }
        previous = i;
    }
    
    return status;
}

bool testTwoProducerSingleConsumer()
{
    int result = 0;

    std::atomic<bool> go { false };

    std::thread producer1(
        [&](){
            while(!go);
            for(Msg i = 3; i < iterations; i += 2)
            {
                PUT p(buf);
                p = i;
            }
            std::cerr << "Prod 1 done" << std::endl;
        });

    std::thread producer2(
        [&](){
            while(!go);
            for(Msg i = 2; i < iterations; i += 2)
            {
                PUT p(buf);
                p = i;
            }
            std::cerr << "Prod 2 done" << std::endl;
        });
    

    std::thread consumer(
        [&](){
            while(!go);
            Msg oldOdd = 1;
            Msg oldEven = 0;
            for(size_t i = 1; i < iterations - 5; ++i)
            {
                Msg msg;
                {
                    msg = GET(buf);
                }

                Msg& old = msg & 0x1L ? oldOdd : oldEven;

                long diff = (msg - old) - 2;
                result += (diff * diff);
                old = msg;
                msgs[i] = msg;                
            }
            std::cerr << "Cons done" << std::endl;
        });

    go = true;

    producer1.join();
    producer2.join();
    consumer.join();

    std::cout << "result: " << result << std::endl;
    std::cout << "putSpinCount    = " << putSpinCount << std::endl;
    std::cout << "getSpinCount    = " << getSpinCount << std::endl; 
    std::cout << "cursorSpinCount = " << cursorSpinCount << std::endl;

    bool status = result == 0;
    
    return status;
}

int main()
{
    bool status = true;
    status = testSingleProducerSingleConsumer();
    status = testTwoProducerSingleConsumer() && status;
    
    return status ? 0 : 1;
}
