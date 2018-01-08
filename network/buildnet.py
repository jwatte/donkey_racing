from __future__ import division, print_function
import numpy as np
import os
import sys
import time
import lmdb
import shutil
import traceback
import math
from netdef import *
import scipy.misc

from caffe2.python import core, model_helper, net_drawer, workspace, visualize, brew
import caffe2.python.predictor.predictor_exporter as pe
import caffe2.python.predictor.predictor_py_utils as pred_utils
from caffe2.python.predictor_constants import predictor_constants as pc
from caffe2.proto import caffe2_pb2

training = True

load_checkpoint=None
load_trained=None
load_trained="load_trained_8.crunk"

if len(sys.argv) > 1:
    try:
        load_checkpoint = sys.argv[1]
        if load_checkpoint.find(".crunk") > 0:
            load_trained = load_checkpoint
            load_checkpoint = None
        if len(sys.argv > 2):
            netdef.iter_val = int(sys.argv[2])
        if len(sys.argv > 3):
            netdef.LEARN_RATE = float(sys.argv[3])
        if netdef.LEARN_RATE >= 0:
            print("LEARN_RATE must be < 0")
            sys.exit(1)
    except:
        print("Usage: buildnet.py [checkpoint iter lr]")
        sys.exit(1)

# If you would like to see some really detailed initializations,
# you can change --caffe2_log_level=0 to --caffe2_log_level=-1
core.GlobalInit(['caffe2', '--caffe2_log_level=0'])
proto_folder = os.path.join(root_folder, 'proto')

print('root_folder=%s' % (root_folder,))
print('data_folder=%s' % (data_folder,))
print('proto_folder=%s' % (proto_folder,))

def save_protobufs(train_model, test_model, deploy_model):
    try:
        os.mkdir(proto_folder)
    except:
        pass
    with open(os.path.join(proto_folder, "train_net.pbtxt"), 'w') as fid:
            fid.write(str(train_model.net.Proto()))
    with open(os.path.join(proto_folder, "train_init_net.pbtxt"), 'w') as fid:
            fid.write(str(train_model.param_init_net.Proto()))
    with open(os.path.join(proto_folder, "test_net.pbtxt"), 'w') as fid:
            fid.write(str(test_model.net.Proto()))
    with open(os.path.join(proto_folder, "test_init_net.pbtxt"), 'w') as fid:
            fid.write(str(test_model.param_init_net.Proto()))
    with open(os.path.join(proto_folder, "deploy_net.pbtxt"), 'w') as fid:
            fid.write(str(deploy_model.net.Proto()))
    print("Protocol buffers files have been created in your root folder: " + proto_folder)

def load_model(pathname):
    workspace.ResetWorkspace(root_folder)
    pathname = os.path.join(root_folder, "donkey_model_protos.lmdb")
    LMDB_MAP_SIZE = 1 << 29
    env = lmdb.open(root_folder, map_size=LMDB_MAP_SIZE)
    with env.begin(write=False) as txn:
        for key, value in txn.cursor():
            if key == ":NET":
                nd = caffe_pb2.NetDef()
                nd.SerializeFromString(value)
                
    predict_net = pe.prepare_prediction_net(pathname, "minidb")
    return predict_net

def run_inference(data, predict_net):
    workspace.FeedBlob("input", data)
    workspace.RunNetOnce(predict_net)
    output = workspace.FetchBlob("output")
    return output[0]

device_opts = caffe2_pb2.DeviceOption()
device_opts.device_type = caffe2_pb2.CUDA
device_opts.cuda_gpu_id = 0


(train, test, deploy) = build_networks()
save_protobufs(train, test, deploy)


def is_param_name(p):
    return p[:4] == "conv" or p[:2] == "fc" or p[:4] == "relu" or p[:4] == "pool" or p[:7] == "squeeze"

def add_noise():
    for p in deploy.params:
        p = str(p)
        if is_param_name(p):
            b = workspace.FetchBlob(p)
            rand = np.random.rand(*b.shape).astype(np.float32)
            rand += -0.5
            rand *= 0.1
            b *= 0.95
            b += rand
            workspace.FeedBlob(p, b, device_opts)
            print("Added noise to " + p)
            sys.stderr.write("Added noise to " + p + "\n")
            

def init_random():
    for p in deploy.params:
        p = str(p)
        if is_param_name(p):
            b = workspace.FetchBlob(p)
            rand = np.random.rand(*b.shape).astype(np.float32)
            rand = np.add(rand, -0.5)
            workspace.FeedBlob(p, rand, device_opts)

INPUT_HEIGHT=146
INPUT_WIDTH=146

if training:
    with open('training.log', 'w') as logfile:
        train.RunAllOnGPU()
        workspace.RunNetOnce(train.param_init_net)
        workspace.CreateNet(train.net, overwrite=False)
        if load_checkpoint:
            load = model_helper.ModelHelper(name="load_checkpoint")
            load.Load([], [], db=load_checkpoint, db_type="lmdb", load_all=1, keep_device=1, absolute_path=0)
            workspace.RunNetOnce(load.net)
            workspace.FeedBlob('iter', [iter_val])
            save_trained_model(deploy)
        if load_trained:
            load_crunk(load_trained, device_opts)
        loss = np.zeros(train_iters)
        start = time.time()
        name = train.net.Proto().name
        numstraight = 0
        i = 0
        while i < train_iters:
            workspace.RunNet(name)
            if i == 0:
                realstart = time.time()
            loss[i] = workspace.FetchBlob('avgloss')
            if i % 200 == 0:
                LR = workspace.FetchBlob('LR')
                stop = time.time()
                j = i
                if j == 0: j = 1
                st = workspace.FetchBlob('output')
                steer = st[0,0]
                lb = workspace.FetchBlob('label')
                label = lb[0,0]
                outputs = []
                for q in st:
                    outputs.append(q[0])
                avg = sum(outputs)/len(outputs)
                nstraight = sum((abs(x-avg) < 0.02) for x in outputs)
                if nstraight == len(outputs):
                    numstraight += 1
                    print("\niteration " + str(i) + " steering averages at: " + str(avg))
                else:
                    numstraight = 0
                if numstraight > 19:
                    logfile.write("\ntwenty iteration with straight-ahead steering in a row -- adding noise!\n")
                    print("\ntwenty iteration with straight-ahead steering in a row -- adding noise!")
                    add_noise()
                    numstraight = 0
                sys.stderr.write("iter %7d loss %8.6f lr %8.6f speed %6.2f remain %7.2f   steer %5.2f label %5.2f  nstraight %3d        \r" %
                        (i, loss[i], LR, 200.0/(stop-start), (stop-realstart)/j*(train_iters-i), steer, label, nstraight))
                logfile.write("iter %d loss %.6f lr %.6f speed %.2f remain %.2f   steer %.2f label %.2f   \n" %
                        (i, loss[i], LR, 200.0/(stop-start), (stop-realstart)/j*(train_iters-i), steer, label))
                if math.isnan(loss[i]) or math.isnan(steer):
                        logfile.write('\nNaN detected -- model diverges. Emergency brake.\n')
                        print('\nNaN detected -- model diverges. Emergency brake.')
                        sys.exit(4)
                # if (i % 5000 == 0):
                #     for j in deploy.params:
                #         ba = workspace.FetchBlob(j)
                #         if len(ba.shape) == 4:
                #             for q in range(0, ba.shape[0]):
                #                 for w in range(0, ba.shape[1]):
                #                     scipy.misc.imsave('param_%d_%s_%d_%d.jpg' % (i, j, q, w), ba[q][w])
                #         if len(ba.shape) == 3:
                #             for q in range(0, ba.shape[0]):
                #                 scipy.misc.imsave('param_%d_%s_%d.jpg' % (i, j, q), ba[q])
                #         if len(ba.shape) == 2:
                #             scipy.misc.imsave('param_%d_%s.jpg' % (i, j), ba)
                start = stop
            i = i + 1
        logfile.write('\nFinished at %d iterations.\n' % (train_iters,))
        print('\nFInished at %d iterations.' % (train_iters,))
    try:
        save_trained_model(deploy)
    except:
        print('exception while saving model:\n' + traceback.format_exc())
    print(loss)
else:
    a = np.zeros((1,1,INPUT_HEIGHT,INPUT_WIDTH), np.float32)
    a[0][0][100][30] = 1.0
    a[0][0][50][30] = 1.0
    a[0][0][100][20] = 1.0
    a[0][0][50][20] = 1.0
    workspace.FeedBlob("input", a)
    workspace.RunNetOnce(train.param_init_net)
    workspace.CreateNet(deploy.net, overwrite=True)
    start = time.time()
    for i in range(0, 100):
        i = run_inference(a, deploy.net)
    stop = time.time()
    print("Time per inference: %f seconds\n" % ((stop-start)/100.0,))
    print(repr(i))

