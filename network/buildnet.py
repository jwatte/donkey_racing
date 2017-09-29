from __future__ import division, print_function
import numpy as np
import os
import sys
import time
import lmdb
import shutil
import traceback
import math

from caffe2.python import core, model_helper, net_drawer, workspace, visualize, brew
import caffe2.python.predictor.predictor_exporter as pe
import caffe2.python.predictor.predictor_py_utils as pred_utils
from caffe2.python.predictor_constants import predictor_constants as pc
from caffe2.proto import caffe2_pb2

use_lmdb = False
training = True
train_iters = 1000000


# If you would like to see some really detailed initializations,
# you can change --caffe2_log_level=0 to --caffe2_log_level=-1
core.GlobalInit(['caffe2', '--caffe2_log_level=0'])
#root_folder = "/home/jwatte/trainingdata/donkey_racing/network"
root_folder = "/home/jwatte/trainingdata"
#data_folder = "/home/jwatte/trainingdata/2017-09-07-8am-park"
data_folder = os.path.join(root_folder, 'temp_oakland')
proto_folder = os.path.join(root_folder, 'proto')

print('root_folder=%s' % (root_folder,))
print('data_folder=%s' % (data_folder,))
print('proto_folder=%s' % (proto_folder,))

def AddInput(model, batch_size, db, db_type):
    data_uint8, label = model.TensorProtosDBInput(
        [], ["data_uint8", "label"], batch_size=batch_size,
        db=db, db_type=db_type)
    data = model.Cast(data_uint8, "data_cast", to=core.DataType.FLOAT)
    data = model.Scale(data, data, scale=float(1./255))
    data = model.StopGradient(data, data)
    return data, label

def AddNetModel(model, data):
    # 180x60 -> 178x58
    conv1 = brew.conv(model, data, 'conv1', dim_in=1, dim_out=8, kernel=3)
    # 178x58 -> 89x29
    pool1 = brew.max_pool(model, conv1, 'pool1', kernel=2, stride=2)
    relu1 = brew.relu(model, pool1, 'relu1')
    # 89x29 -> 86x26
    conv2 = brew.conv(model, relu1, 'conv2', dim_in=8, dim_out=16, kernel=4)
    # 86x26 -> 43x13
    pool2 = brew.max_pool(model, conv2, 'pool2', kernel=2, stride=2)
    relu2 = brew.relu(model, pool2, 'relu2')
    # 43x13 -> 40x10
    conv3 = brew.conv(model, relu2, 'conv3', dim_in=16, dim_out=24, kernel=4)
    # 40x10 -> 20x5
    pool3 = brew.max_pool(model, conv3, 'pool3', kernel=2, stride=2)
    relu3 = brew.relu(model, pool3, 'relu3')
    # 20x5 -> 18x3
    conv4 = brew.conv(model, relu3, 'conv4', dim_in=24, dim_out=48, kernel=3)
    # 18x3 -> 17x2
    pool4 = brew.max_pool(model, conv4, 'pool4', kernel=2)
    relu4 = brew.relu(model, pool4, 'relu4')
    # 17x2 -> 128
    fc5 = brew.fc(model, relu4, 'fc5', dim_in=17*2*48, dim_out=128)
    relu5 = brew.relu(model, fc5, 'relu5')
    # 128 -> 2
    output = brew.fc(model, relu5, 'output', dim_in=128, dim_out=2)
    return output

def AddAccuracy(model, output, label):
    accuracy = brew.accuracy(model, [output, label], "accuracy")
    return accuracy

def AddTrainingOperators(model, output, label):
    #xent = model.LabelCrossEntropy([output, label], 'xent')
    xdiff = model.Sub([output, label], "xdiff")
    sqdiff = model.DotProduct([xdiff, xdiff], "sqdiff")
    loss = model.AveragedLoss(sqdiff, "loss")
    #AddAccuracy(model, output, label)
    model.AddGradientOperators([loss])
    ITER = brew.iter(model, "iter")
    stepsize = int(train_iters / 30000)
    if stepsize < 1:
        stepsize = 1
    if stepsize > 25:
        stepsize = 25
    LR = model.LearningRate(
        ITER, "LR", base_lr=-0.06, policy="step", stepsize=1, gamma=0.99997 )
    ONE = model.param_init_net.ConstantFill([], "ONE", shape=[1], value=1.0)
    for param in model.params:
        param_grad = model.param_to_grad[param]
        model.WeightedSum([param, ONE, param_grad, LR], param)
    model.Checkpoint([ITER] + model.params, [],
        db="checkpoint_%06d.lmdb", db_type="lmdb", every=10000)

def AddBookkeepingOperators(model):
    #model.Print('accuracy', [], to_file=1)
    model.Print('loss', [], to_file=1)
    for param in model.params:
        model.Summarize(param, [], to_file=1)
    model.Summarize(model.param_to_grad[param], [], to_file=1)

def build_networks():
    arg_scope = {"order": "NCHW", "use_cudnn":True}
    train_model = model_helper.ModelHelper(name="donkey_train", arg_scope=arg_scope)
    dbpath = os.path.join(data_folder, 'train')
    print('dbpath = %s' % (dbpath,))
    data, label = AddInput(
        train_model, batch_size=64,
        db=dbpath,
        db_type='lmdb')
    output = AddNetModel(train_model, data)
    AddTrainingOperators(train_model, output, label)
    AddBookkeepingOperators(train_model)

    test_model = model_helper.ModelHelper(
        name="donkey_test", arg_scope=arg_scope, init_params=False)
    data, label = AddInput(
        test_model, batch_size=1,
        db=os.path.join(data_folder, 'test'),
        db_type='lmdb')
    output = AddNetModel(test_model, data)
    #AddAccuracy(test_model, output, label)

    deploy_model = model_helper.ModelHelper(
        name="donkey_deploy", arg_scope=arg_scope, init_params=False)
    AddNetModel(deploy_model, "input")

    return (train_model, test_model, deploy_model)

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

def write_block(fil, nam, info, blob, bsize):
    fil.write("%s\n" % (nam,))
    fil.write("%s\n" % (info,))
    fil.write("%d\n" % (bsize,))
    fil.write(blob)
    fil.write("\n-\n")

def save_trained_model(deploy_model):
    savepath = os.path.join(root_folder, "donkey_model_protos.crunk")
    print('savepath = %s' % (savepath,))
    try:
        shutil.rmtree(savepath)
    except:
        pass
    if use_lmdb:
        LMDB_MAP_SIZE = 1 << 29
        env = lmdb.open(savepath, map_size=LMDB_MAP_SIZE)
        with env.begin(write=True) as txn:
            txn.put(':NET', deploy_model.net.Proto().SerializeToString())
            parameters = [str(b) for b in deploy_model.params]
            for i in parameters:
                b = workspace.FetchBlob(i)
                print(i + ": " + repr(b.shape))
                tp = caffe2_pb2.TensorProto()
                tp.dims.extend(b.shape)
                tp.data_type = 1
                flat = b.reshape(np.prod(b.shape))
                tp.float_data.extend([x.item() for x in flat.flat])
                txn.put(i, tp.SerializeToString())
    else:
        with open(savepath, 'wb') as of:
            of.write("crunk 1.0\n")
            ss = deploy_model.net.Proto().SerializeToString()
            write_block(of, ':NET', '', ss, len(ss))
            parameters = [str(b) for b in deploy_model.params]
            for i in parameters:
                b = workspace.FetchBlob(i)
                print(i + ": " + repr(b.shape))
                write_block(of, i, repr(b.shape), b, b.size * b.itemsize)

    print("The deploy model is saved to: " + savepath)

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


load_checkpoint=None

if len(sys.argv) > 1:
    load_checkpoint = sys.argv[1]

if training:
    train.RunAllOnGPU()
    test.RunAllOnGPU()
    workspace.RunNetOnce(train.param_init_net)
    workspace.FeedBlob('input', np.zeros((1,60,180)))
    workspace.CreateNet(deploy.net, overwrite=True)
    workspace.CreateNet(train.net, overwrite=True)
    if load_checkpoint:
        load = model_helper.ModelHelper(name="load_checkpoint")
        load.Load([], [], db=load_checkpoint, db_type="lmdb", load_all=1, keep_device=1, absolute_path=0)
        workspace.RunNetOnce(load.net)
        save_trained_model(deploy)
        workspace.FeedBlob('LR', np.array([0.03]))
    loss = np.zeros(train_iters)
    start = time.time()
    for i in range(train_iters):
        workspace.RunNet(train.net.Proto().name)
        if i == 0:
            realstart = time.time()
        loss[i] = workspace.FetchBlob('loss')
        if i % 200 == 0:
            LR = workspace.FetchBlob('LR')
            stop = time.time()
            j = i
            if j == 0: j = 1
            st = workspace.FetchBlob('output')
            steer = st[0][0]
            lb = workspace.FetchBlob('label')
            label = lb[0][0]
            sys.stderr.write("iter %d loss %.6f lr %.6f speed %.2f remain %.2f   steer %.2f label %.2f   \r" %
                    (i, loss[i], LR, 200.0/(stop-start), (stop-realstart)/j*(train_iters-i), steer, label))
            if math.isnan(loss[i]) or math.isnan(steer):
                    print('\nNaN detected -- model diverges. Emergency brake.')
                    sys.exit(4)
            start = stop
    print('\nFInished at %d iterations.' % (train_iters,))
    try:
        save_trained_model(deploy)
    except:
        print('exception while saving model:\n' + traceback.format_exc())
    print(loss)
else:
    a = np.zeros((1,1,180,60), np.float32)
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

