from __future__ import division, print_function
import numpy as np
import os
import time

from caffe2.python import core, model_helper, net_drawer, workspace, visualize, brew

# If you would like to see some really detailed initializations,
# you can change --caffe2_log_level=0 to --caffe2_log_level=-1
core.GlobalInit(['caffe2', '--caffe2_log_level=0'])
print("Necessities imported!")
root_folder = "/usr/local/src/donkey_racing/network"
data_folder = os.path.join(root_folder, 'data')
proto_folder = os.path.join(root_folder, 'proto')

def AddInput(model, batch_size, db, db_type):
    data_uint8, label = model.TensorProtosDBInput(
        [], ["data_uint8", "label"], batch_size=batch_size,
        db=db, db_type=db_type)
    data = model.Cast(data_uint8, "data_cast", to=core.DataType.FLOAT)
    data = model.Scale(data, data, scale=float(1./255))
    data = model.StopGradient(data, data)
    return data, label

# input: 2C * 221W * 93H
def AddNetModel(model, data):
    # 221x93 -> 218x90
    conv1 = brew.conv(model, data, 'conv1', dim_in=2, dim_out=8, kernel=4)
    # 218x90 -> 109x45
    pool1 = brew.max_pool(model, conv1, 'pool1', kernel=2, stride=2)
    # 109x45 -> 106x42
    conv2 = brew.conv(model, pool1, 'conv2', dim_in=8, dim_out=16, kernel=4)
    # 106x42 -> 53x21
    pool2 = brew.max_pool(model, conv2, 'pool2', kernel=2, stride=2)
    relu2 = brew.relu(model, pool2, 'relu2')
    # 53x21 -> 50x18
    conv3 = brew.conv(model, relu2, 'conv3', dim_in=16, dim_out=32, kernel=4)
    # 50x18 -> 25x9
    pool3 = brew.max_pool(model, conv3, 'pool3', kernel=2, stride=2)
    # 25x9 -> 22x6
    conv4 = brew.conv(model, pool3, 'conv4', dim_in=32, dim_out=64, kernel=4)
    # 22x6 -> 11x3
    pool4 = brew.max_pool(model, conv4, 'pool4', kernel=2, stride=2)
    relu4 = brew.relu(model, pool4, 'relu4')
    # 11x3 -> 128
    fc4 = brew.fc(model, relu4, 'fc4', dim_in=64*11*3, dim_out=128)
    fc4 = brew.relu(model, fc4, fc4)
    # 128 -> 2
    output = brew.fc(model, fc4, 'output', dim_in=128, dim_out=2)
    return output

def AddAccuracy(model, output, label):
    accuracy = brew.accuracy(model, [output, label], "accuracy")
    return accuracy

def AddTrainingOperators(model, output, label):
    xent = model.LabelCrossEntropy([output, label], 'xent')
    loss = model.AveragedLoss(xent, "loss")
    AddAccuracy(model, output, label)
    model.AddGradientOperators([loss])
    ITER = brew.iter(model, "iter")
    LR = model.LearningRate(
        ITER, "LR", base_lr=-0.1, policy="step", stepsize=1, gamma=0.999 )
    ONE = model.param_init_net.ConstantFill([], "ONE", shape=[1], value=1.0)
    for param in model.params:
        param_grad = model.param_to_grad[param]
        model.WeightedSum([param, ONE, param_grad, LR], param)

def AddBookkeepingOperators(model):
    model.Print('accuracy', [], to_file=1)
    model.Print('loss', [], to_file=1)
    for param in model.params:
        model.Summarize(param, [], to_file=1)
    model.Summarize(model.param_to_grad[param], [], to_file=1)

def build_networks():
    arg_scope = {"order": "NCHW"}
    train_model = model_helper.ModelHelper(name="donkey_train", arg_scope=arg_scope)
    data, label = AddInput(
        train_model, batch_size=64,
        db=os.path.join(data_folder, 'donkey-train-nchw-lmdb'),
        db_type='lmdb')
    output = AddNetModel(train_model, data)
    AddTrainingOperators(train_model, output, label)
    AddBookkeepingOperators(train_model)

    test_model = model_helper.ModelHelper(
        name="donkey_test", arg_scope=arg_scope, init_params=False)
    data, label = AddInput(
        test_model, batch_size=100,
        db=os.path.join(data_folder, 'donkey-test-nchw-lmdb'),
        db_type='lmdb')
    output = AddNetModel(test_model, data)
    AddAccuracy(test_model, output, label)

    deploy_model = model_helper.ModelHelper(
        name="donkey_deploy", arg_scope=arg_scope, init_params=False)
    AddNetModel(deploy_model, "input")

    return (train_model, test_model, deploy_model)

def save_protobufs(train_model, test_model, deploy_model):
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

def train_model(train_model, iterations=200):
    workspace.ResetWorkspace(root_folder)
    workspace.RunNetOnce(train_model.param_init_net)
    workspace.CreateNet(train_model.net, overwrite=True)
    accuracy = np.zeros(iterations)
    loss = np.zeros(iterations)
    for i in range(iterations):
        workspace.RunNet(train_model.net)
        accuracy[i] = workspace.FetchBlob('accuracy')
        loss[i] = workspace.FetchBlob('loss')
        print('Iteration %d: Accuracy: %f, Loss: %f' % (i, accuracy[i], loss[i]))

def save_trained_model(deploy_model):
    pe_meta = pe.PredictorExportMeta(
        predict_net=deploy_model.net.Proto(),
        parameters=[str(b) for b in deploy_model.params], 
        inputs=["input"],
        outputs=["output"])
    savepath = os.path.join(root_folder, "donkey_model.minidb")
    pe.save_to_db("minidb", savepath, pe_meta)
    print("The deploy model is saved to: " + savepath)

def load_model(pathname):
    workspace.ResetWorkspace(root_folder)
    pathname = os.path.join(root_folder, "donkey_model.minidb")
    predict_net = pe.prepare_prediction_net(pathname, "minidb")
    return predict_net

def run_inference(data, predict_net):
    workspace.FeedBlob("input", data)
    workspace.RunNetOnce(predict_net)
    output = workspace.FetchBlob("output")
    return output[0]

(train, test, deploy) = build_networks()
save_protobufs(train, test, deploy)


workspace.RunNetOnce(train.param_init_net)
workspace.CreateNet(train.net, overwrite=True)
a = np.zeros((1,2,221,93), np.float32)
start = time.time()
for i in range(0, 100):
    i = run_inference(a, deploy.net)
stop = time.time()
print("Time per inference: %f seconds\n" % ((stop-start)/100.0,))
print(repr(i))

