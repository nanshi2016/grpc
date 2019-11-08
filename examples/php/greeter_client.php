<?php
/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

// php:generate protoc --proto_path=./../protos   --php_out=./   --grpc_out=./ --plugin=protoc-gen-grpc=./../../bins/opt/grpc_php_plugin ./../protos/helloworld.proto

require dirname(__FILE__).'/vendor/autoload.php';

@include_once dirname(__FILE__).'/Helloworld/GreeterClient.php';
@include_once dirname(__FILE__).'/Helloworld/HelloReply.php';
@include_once dirname(__FILE__).'/Helloworld/HelloRequest.php';
@include_once dirname(__FILE__).'/Helloworld/NestedMessage.php';
@include_once dirname(__FILE__).'/GPBMetadata/Helloworld.php';

function greet($name)
{
    $len = strlen($name)/100;
    $nestedMessages = array();
    for ($i = 0; $i < 10; ++$i) {
        $messages = array();
        for ($j = 0; $j < 10; ++$j) {
            $messages[] = substr($name, $i * 10 + $j, $len);
        }
        $nestedMessages[] = new Helloworld\NestedMessage(['messages' => $messages]);
    }

    $client = new Helloworld\GreeterClient('localhost:50051', [
        'credentials' => Grpc\ChannelCredentials::createInsecure(),
        'grpc.max_send_message_length' => 8*1024*1024,
        'grpc.max_receive_message_length' => 8*1024*1024,
    ]);
    //$request = new Helloworld\HelloRequest();
    //$request->setName($name);
    $request = new Helloworld\HelloRequest([
        "name" => "perf test",
        "nestedMessages" => $nestedMessages,
    ]);
    list($reply, $status) = $client->SayHello($request)->wait();
    // var_dump($status);
    //$message = $reply->getMessage();
    $message = "".$reply->getMessage();
    $message = $message."\n ".$reply->getNestedMessages()[0]->getMessages()[0];
    $message = $message."\n "
               .count($reply->getNestedMessages())
               ." x ".count($reply->getNestedMessages()[0]->getMessages());

    return $message;
}

//$name = !empty($argv[1]) ? $argv[1] : 'world';
$stdinFn = fopen('php://stdin','r');
$stdin = '';
if (!stream_isatty($stdinFn)) {
    $stdin = stream_get_contents($stdinFn);
}
$name = !empty($argv[1]) ? $argv[1] : (!empty($stdin) ? $stdin : 'world');

echo greet($name)."\n";
